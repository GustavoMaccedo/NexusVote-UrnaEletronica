// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
//! Sigilo de conteúdo: `data` pode ser selado (AES-GCM) — neste protótipo

use crate::canonical_json::to_canonical_bytes;
use crate::crypto::{self, Keypair};
use crate::hash_chain::{genesis_hash, line_hash};
use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum JournalError {
    #[error("seq fora de ordem")]
    BadSeq,
    #[error("cadeia quebrada")]
    BadChain,
    #[error("assinatura inválida")]
    BadSig,
    #[error("json: {0}")]
    Json(String),
    #[error("cripto: {0}")]
    Crypto(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SealedLine {
    pub v: u32,
    pub seq: u64,
    pub prev: String,
    pub hash: String,
    pub data: serde_json::Value,
    #[serde(rename = "pub")]
    pub pubkey: String,
    pub sig: String,
}

pub struct JournalWriter {
    prev: [u8; 32],
    next_seq: u64,
    keypair: Keypair,
    pubkey_hex: String,
}

impl JournalWriter {
    pub fn new(keypair: Keypair) -> Self {
        let pubkey_hex = keypair.verifying_hex();
        Self {
            prev: genesis_hash(),
            next_seq: 1,
            keypair,
            pubkey_hex,
        }
    }

    pub fn pubkey_hex(&self) -> &str {
        &self.pubkey_hex
    }

    pub fn seal(&mut self, data: serde_json::Value) -> Result<SealedLine, JournalError> {
        let canon = to_canonical_bytes(&data).map_err(|e| JournalError::Json(e.to_string()))?;
        let h = line_hash(&self.prev, self.next_seq, &canon);
        let (prev_hex, hash_hex) = (crypto::to_hex(&self.prev), crypto::to_hex(&h));

        let sign_msg = serde_json::json!({
            "v": crate::SECURE_JOURNAL_VERSION,
            "seq": self.next_seq,
            "prev": prev_hex,
            "hash": hash_hex,
            "data": data,
        });
        let sign_bytes =
            to_canonical_bytes(&sign_msg).map_err(|e| JournalError::Json(e.to_string()))?;
        let sig = self.keypair.sign(&sign_bytes);

        let line = SealedLine {
            v: crate::SECURE_JOURNAL_VERSION,
            seq: self.next_seq,
            prev: prev_hex,
            hash: hash_hex,
            data,
            pubkey: self.pubkey_hex.clone(),
            sig: crypto::to_hex(&sig),
        };
        self.prev = h;
        self.next_seq += 1;
        Ok(line)
    }
}

pub fn verify_lines(lines: &[SealedLine]) -> Result<(), JournalError> {
    let mut prev = genesis_hash();
    for (idx, l) in lines.iter().enumerate() {
        let expected_seq = (idx as u64) + 1;
        if l.v != crate::SECURE_JOURNAL_VERSION {
            return Err(JournalError::Json("versão".into()));
        }
        if l.seq != expected_seq {
            return Err(JournalError::BadSeq);
        }
        if l.prev != crypto::to_hex(&prev) {
            return Err(JournalError::BadChain);
        }
        let canon = to_canonical_bytes(&l.data).map_err(|e| JournalError::Json(e.to_string()))?;
        let h = line_hash(&prev, l.seq, &canon);
        if l.hash != crypto::to_hex(&h) {
            return Err(JournalError::BadChain);
        }
        let sign_msg = serde_json::json!({
            "v": l.v, "seq": l.seq, "prev": l.prev, "hash": l.hash, "data": l.data,
        });
        let sign_bytes =
            to_canonical_bytes(&sign_msg).map_err(|e| JournalError::Json(e.to_string()))?;
        let pk = crypto::from_hex(&l.pubkey).map_err(|e| JournalError::Crypto(e.to_string()))?;
        let sig = crypto::from_hex(&l.sig).map_err(|e| JournalError::Crypto(e.to_string()))?;
        crypto::verify_ed25519(&pk, &sign_bytes, &sig).map_err(|_| JournalError::BadSig)?;
        prev = h;
    }
    Ok(())
}

pub fn line_to_jsonl(l: &SealedLine) -> Result<String, JournalError> {
    serde_json::to_string(l).map_err(|e| JournalError::Json(e.to_string()))
}

pub fn line_from_jsonl(s: &str) -> Result<SealedLine, JournalError> {
    if s.len() > 65_536 {
        return Err(JournalError::Json("linha > 64KiB".into()));
    }
    serde_json::from_str(s).map_err(|e| JournalError::Json(e.to_string()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn seal_verify_roundtrip() {
        let kp = Keypair::generate().unwrap();
        let mut w = JournalWriter::new(kp);
        let l1 = w
            .seal(json!({"office":"vereador","kind":"nominal","ref":"12345"}))
            .unwrap();
        let l2 = w.seal(json!({"office":"prefeito","kind":"blank"})).unwrap();
        assert!(verify_lines(&[l1, l2]).is_ok());
    }

    #[test]
    fn detects_rewrite_and_rollback() {
        let kp = Keypair::generate().unwrap();
        let mut w = JournalWriter::new(kp);
        let l1 = w.seal(serde_json::json!({"a":1})).unwrap();
        let l2 = w.seal(serde_json::json!({"a":2})).unwrap();
        assert!(verify_lines(&[l2]).is_err());
        let mut bad = l1;
        bad.data = serde_json::json!({"a":999});
        assert!(verify_lines(&[bad]).is_err());
    }

    #[test]
    fn jsonl_roundtrip() {
        let kp = Keypair::generate().unwrap();
        let mut w = JournalWriter::new(kp);
        let l = w.seal(serde_json::json!({"x":true})).unwrap();
        let s = line_to_jsonl(&l).unwrap();
        assert!(!s.contains('\n'));
        let back = line_from_jsonl(&s).unwrap();
        assert_eq!(back.seq, 1);
    }
}
