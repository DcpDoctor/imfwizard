use std::io::Write;
use std::path::Path;

/// Write a PKL (Packing List) XML file.
pub fn write_pkl(
    path: &Path,
    pkl_uuid: &str,
    cpl_uuid: &str,
    cpl_path: &Path,
) -> std::io::Result<()> {
    let cpl_hash = postkit::hash::hash_file(cpl_path, postkit::hash::HashAlgorithm::Sha1)
        .map(|h| h.base64)
        .unwrap_or_default();
    let cpl_size = std::fs::metadata(cpl_path).map(|m| m.len()).unwrap_or(0);

    let mut f = std::fs::File::create(path)?;
    writeln!(f, r#"<?xml version="1.0" encoding="UTF-8"?>"#)?;
    writeln!(
        f,
        r#"<PackingList xmlns="http://www.smpte-ra.org/schemas/2067-2/2016">"#
    )?;
    writeln!(f, "  <Id>urn:uuid:{pkl_uuid}</Id>")?;
    writeln!(f, "  <Creator>IMF Wizard</Creator>")?;
    writeln!(f, "  <AssetList>")?;
    writeln!(f, "    <Asset>")?;
    writeln!(f, "      <Id>urn:uuid:{cpl_uuid}</Id>")?;
    writeln!(f, "      <Hash>{cpl_hash}</Hash>")?;
    writeln!(f, "      <Size>{cpl_size}</Size>")?;
    writeln!(f, "      <Type>text/xml</Type>")?;
    writeln!(f, "    </Asset>")?;
    writeln!(f, "  </AssetList>")?;
    writeln!(f, "</PackingList>")?;
    Ok(())
}
