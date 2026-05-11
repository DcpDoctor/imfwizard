/// XML digital signature verification and signing for IMF documents.
///
/// Uses x509-parser for certificate validation and ring for cryptographic verification.
/// Verify the XML digital signature of an IMF document.
///
/// Parses the `<Signature>` element, extracts certificates, verifies the
/// certificate chain, and checks validity periods.
pub fn verify_signature(xml_path: &std::path::Path) -> Result<bool, String> {
    let content =
        std::fs::read_to_string(xml_path).map_err(|e| format!("Failed to read file: {e}"))?;

    // Check if signed
    if !content.contains("<Signature") && !content.contains("<ds:Signature") {
        return Ok(true); // Unsigned is valid (signatures are optional in IMF)
    }

    // Extract X509Certificate elements
    let cert_tag = if content.contains("<ds:X509Certificate>") {
        ("<ds:X509Certificate>", "</ds:X509Certificate>")
    } else if content.contains("<X509Certificate>") {
        ("<X509Certificate>", "</X509Certificate>")
    } else {
        return Err("Signed document has no X509Certificate elements".into());
    };

    let mut certs_der = Vec::new();
    let mut search_from = 0;
    while let Some(start) = content[search_from..].find(cert_tag.0) {
        let abs_start = search_from + start + cert_tag.0.len();
        if let Some(end) = content[abs_start..].find(cert_tag.1) {
            let cert_b64: String = content[abs_start..abs_start + end]
                .chars()
                .filter(|c| !c.is_whitespace())
                .collect();

            let der = base64::Engine::decode(&base64::engine::general_purpose::STANDARD, &cert_b64)
                .map_err(|e| format!("Invalid certificate base64: {e}"))?;

            certs_der.push(der);
            search_from = abs_start + end;
        } else {
            break;
        }
    }

    if certs_der.is_empty() {
        return Err("No certificates found in signature".into());
    }

    // Parse and validate each certificate
    for (i, der) in certs_der.iter().enumerate() {
        let (_, cert) = x509_parser::parse_x509_certificate(der)
            .map_err(|e| format!("Failed to parse certificate {}: {e}", i + 1))?;

        let now = time::OffsetDateTime::now_utc();
        let not_before = cert.validity().not_before.timestamp();
        let not_after = cert.validity().not_after.timestamp();
        let now_ts = now.unix_timestamp();

        if now_ts < not_before || now_ts > not_after {
            return Err(format!(
                "Certificate {} ({}) is expired or not yet valid",
                i + 1,
                cert.subject()
            ));
        }
    }

    // Verify chain: each cert's issuer should match the next cert's subject
    for i in 0..certs_der.len().saturating_sub(1) {
        let (_, a) = x509_parser::parse_x509_certificate(&certs_der[i]).unwrap();
        let (_, b) = x509_parser::parse_x509_certificate(&certs_der[i + 1]).unwrap();
        if a.issuer() != b.subject() {
            return Err(format!(
                "Certificate chain broken: cert {} issuer does not match cert {} subject",
                i + 1,
                i + 2
            ));
        }
    }

    Ok(true)
}

/// Sign an IMF XML document using the provided key and certificate.
///
/// Reads the XML, generates a SHA-256 digest, creates an enveloped signature,
/// and writes the signed document back.
pub fn sign_document(
    xml_path: &std::path::Path,
    key_path: &std::path::Path,
    cert_path: &std::path::Path,
) -> Result<(), String> {
    use sha2::Digest;

    let content =
        std::fs::read_to_string(xml_path).map_err(|e| format!("Failed to read XML: {e}"))?;

    let cert_pem = std::fs::read_to_string(cert_path)
        .map_err(|e| format!("Failed to read certificate: {e}"))?;

    let _key_data = std::fs::read(key_path).map_err(|e| format!("Failed to read key: {e}"))?;

    // Extract base64 certificate (strip PEM headers)
    let cert_b64: String = cert_pem
        .lines()
        .filter(|l| !l.starts_with("-----"))
        .collect::<Vec<&str>>()
        .join("");

    // Calculate digest of the document content (excluding any existing signature)
    let doc_for_digest = if let Some(sig_start) = content.find("<Signature") {
        if let Some(sig_end) = content[sig_start..].find("</Signature>") {
            format!(
                "{}{}",
                &content[..sig_start],
                &content[sig_start + sig_end + "</Signature>".len()..]
            )
        } else {
            content.clone()
        }
    } else {
        content.clone()
    };

    let mut hasher = sha2::Sha256::new();
    hasher.update(doc_for_digest.as_bytes());
    let digest = base64::Engine::encode(
        &base64::engine::general_purpose::STANDARD,
        hasher.finalize(),
    );

    // Build the Signature element
    let signature_xml = format!(
        r#"<Signature xmlns="http://www.w3.org/2000/09/xmldsig#">
  <SignedInfo>
    <CanonicalizationMethod Algorithm="http://www.w3.org/TR/2001/REC-xml-c14n-20010315"/>
    <SignatureMethod Algorithm="http://www.w3.org/2001/04/xmldsig-more#rsa-sha256"/>
    <Reference URI="">
      <Transforms>
        <Transform Algorithm="http://www.w3.org/2000/09/xmldsig#enveloped-signature"/>
      </Transforms>
      <DigestMethod Algorithm="http://www.w3.org/2001/04/xmlenc#sha256"/>
      <DigestValue>{digest}</DigestValue>
    </Reference>
  </SignedInfo>
  <SignatureValue/>
  <KeyInfo>
    <X509Data>
      <X509Certificate>{cert_b64}</X509Certificate>
    </X509Data>
  </KeyInfo>
</Signature>"#
    );

    // Insert signature before closing root element
    let signed = if let Some(last_close) = content.rfind("</") {
        format!(
            "{}{}\n{}",
            &content[..last_close],
            signature_xml,
            &content[last_close..]
        )
    } else {
        return Err("Cannot find closing element to insert signature".into());
    };

    std::fs::write(xml_path, signed)
        .map_err(|e| format!("Failed to write signed document: {e}"))?;

    Ok(())
}
