#!/bin/sh
# Fix tls.rs for rustls 0.22 API

cd /build/gst-meet

# Add imports at the top
sed -i '1i\
#[cfg(all(feature = "tls-insecure", any(feature = "tls-rustls-native-roots", feature = "tls-rustls-webpki-roots")))]\
use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};\
#[cfg(all(feature = "tls-insecure", any(feature = "tls-rustls-native-roots", feature = "tls-rustls-webpki-roots")))]\
use rustls::pki_types::{CertificateDer, ServerName, UnixTime};\
#[cfg(all(feature = "tls-insecure", any(feature = "tls-rustls-native-roots", feature = "tls-rustls-webpki-roots")))]\
use rustls::{DigitallySignedStruct, SignatureScheme};' lib-gst-meet/src/tls.rs

# Add Debug derive
sed -i 's/^struct InsecureServerCertVerifier;/#[derive(Debug)]\nstruct InsecureServerCertVerifier;/' lib-gst-meet/src/tls.rs

# Replace the impl block
cat > /tmp/new_impl.txt <<'EOF'
impl ServerCertVerifier for InsecureServerCertVerifier {
  fn verify_server_cert(
    &self,
    _end_entity: &CertificateDer<'_>,
    _intermediates: &[CertificateDer<'_>],
    _server_name: &ServerName<'_>,
    _ocsp_response: &[u8],
    _now: UnixTime,
  ) -> Result<ServerCertVerified, rustls::Error> {
    Ok(ServerCertVerified::assertion())
  }

  fn verify_tls12_signature(
    &self,
    _message: &[u8],
    _cert: &CertificateDer<'_>,
    _dss: &DigitallySignedStruct,
  ) -> Result<HandshakeSignatureValid, rustls::Error> {
    Ok(HandshakeSignatureValid::assertion())
  }

  fn verify_tls13_signature(
    &self,
    _message: &[u8],
    _cert: &CertificateDer<'_>,
    _dss: &DigitallySignedStruct,
  ) -> Result<HandshakeSignatureValid, rustls::Error> {
    Ok(HandshakeSignatureValid::assertion())
  }

  fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
    vec![
      SignatureScheme::RSA_PKCS1_SHA256,
      SignatureScheme::RSA_PKCS1_SHA384,
      SignatureScheme::RSA_PKCS1_SHA512,
      SignatureScheme::ECDSA_NISTP256_SHA256,
      SignatureScheme::ECDSA_NISTP384_SHA384,
      SignatureScheme::RSA_PSS_SHA256,
      SignatureScheme::RSA_PSS_SHA384,
      SignatureScheme::RSA_PSS_SHA512,
      SignatureScheme::ED25519,
    ]
  }
}
EOF

# Replace from "impl rustls::client::ServerCertVerifier" to the end of file
sed -i '/^impl rustls::client::ServerCertVerifier/,$d' lib-gst-meet/src/tls.rs
cat /tmp/new_impl.txt >> lib-gst-meet/src/tls.rs
