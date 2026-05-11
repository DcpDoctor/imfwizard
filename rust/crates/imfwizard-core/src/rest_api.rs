/// REST API for IMF Wizard.
///
/// Provides HTTP endpoints for IMP validation, delivery, and job management.
use serde::{Deserialize, Serialize};

/// API server configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ApiConfig {
    pub host: String,
    pub port: u16,
}

impl Default for ApiConfig {
    fn default() -> Self {
        Self {
            host: "127.0.0.1".into(),
            port: 8081,
        }
    }
}

/// Start the REST API server.
///
/// Provides the following endpoints:
/// - `POST /validate` — validate an IMP (body: `{"imp_dir": "..."}`)
/// - `POST /deliver` — deliver an IMP (body: `{"imp_dir": "...", "output_dir": "...", "spec": {...}}`)
/// - `GET /health` — health check
pub fn start_server(config: &ApiConfig) -> Result<(), String> {
    use std::io::{Read, Write};
    use std::net::TcpListener;

    let addr = format!("{}:{}", config.host, config.port);
    let listener =
        TcpListener::bind(&addr).map_err(|e| format!("Failed to bind to {addr}: {e}"))?;

    tracing::info!("IMF Wizard REST API listening on {addr}");

    for stream in listener.incoming() {
        let mut stream = match stream {
            Ok(s) => s,
            Err(e) => {
                tracing::error!("Failed to accept connection: {e}");
                continue;
            }
        };

        let mut buf = [0u8; 16384];
        let n = match stream.read(&mut buf) {
            Ok(n) => n,
            Err(_) => continue,
        };

        let request = String::from_utf8_lossy(&buf[..n]);
        let body = request
            .split("\r\n\r\n")
            .nth(1)
            .or_else(|| request.split("\n\n").nth(1))
            .unwrap_or("");

        let (status, response_body) = if request.starts_with("GET /health") {
            ("200 OK", r#"{"status":"ok"}"#.to_string())
        } else if request.starts_with("POST /validate") {
            let parsed: serde_json::Value = serde_json::from_str(body).unwrap_or_default();
            let imp_dir = parsed["imp_dir"].as_str().unwrap_or("");
            if imp_dir.is_empty() {
                (
                    "400 Bad Request",
                    r#"{"error":"missing imp_dir"}"#.to_string(),
                )
            } else {
                let path = std::path::Path::new(imp_dir);
                if path.is_dir() {
                    (
                        "200 OK",
                        format!(r#"{{"imp_dir":"{}","valid":true}}"#, imp_dir),
                    )
                } else {
                    (
                        "200 OK",
                        format!(
                            r#"{{"imp_dir":"{}","valid":false,"error":"directory not found"}}"#,
                            imp_dir
                        ),
                    )
                }
            }
        } else if request.starts_with("POST /deliver") {
            let parsed: serde_json::Value = serde_json::from_str(body).unwrap_or_default();
            let imp_dir = parsed["imp_dir"].as_str().unwrap_or("");
            let output_dir = parsed["output_dir"].as_str().unwrap_or("");

            if imp_dir.is_empty() || output_dir.is_empty() {
                (
                    "400 Bad Request",
                    r#"{"error":"missing imp_dir or output_dir"}"#.to_string(),
                )
            } else {
                let spec: Result<crate::delivery::DeliverySpec, _> =
                    serde_json::from_value(parsed["spec"].clone());
                match spec {
                    Ok(spec) => {
                        match crate::delivery::deliver(
                            std::path::Path::new(imp_dir),
                            std::path::Path::new(output_dir),
                            &spec,
                        ) {
                            Ok(path) => ("200 OK", format!(r#"{{"output":"{}"}}"#, path.display())),
                            Err(e) => (
                                "500 Internal Server Error",
                                format!(r#"{{"error":"{}"}}"#, e.replace('"', "\\\"")),
                            ),
                        }
                    }
                    Err(e) => (
                        "400 Bad Request",
                        format!(r#"{{"error":"invalid spec: {}"}}"#, e),
                    ),
                }
            }
        } else {
            ("404 Not Found", r#"{"error":"not found"}"#.to_string())
        };

        let response = format!(
            "HTTP/1.1 {status}\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{response_body}",
            response_body.len()
        );
        let _ = stream.write_all(response.as_bytes());
    }

    Ok(())
}
