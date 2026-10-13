// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
//! PROTÓTIPO INDEPENDENTE - LABORATÓRIO. Sem validade eleitoral.

#![forbid(unsafe_code)]
#![warn(clippy::all)]

pub mod canonical_json;
pub mod crypto;
pub mod hash_chain;
pub mod nonce_store;
pub mod pin_auth;
pub mod sealed_journal;
pub mod version;

pub const SECURE_JOURNAL_VERSION: u32 = 1;
pub const RULES_PROFILE: &str = "LAB-BR-EDU-v1";
