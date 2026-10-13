#pragma once

// Ponte C++ → Rust (canal seguro 1.3.0-lab).
// PROTÓTIPO DE LABORATÓRIO. Declara a FFI mínima do `nv-secure-ffi`
// (cdylib/staticlib). Quando `WITH_RUST_SECURE=OFF`, o núcleo C++
// segue o caminho legado (journal em claro + Ed25519-demo OpenSSL).

#include <cstddef>
#include <cstdint>

extern "C" {

// 0 ok, negativo = erro (ver `NvErr` no Rust).
int nv_init_sealed_session();
// out precisa de >= 65 bytes. Retorna tamanho (64) ou negativo.
int nv_pubkey(uint8_t* out, size_t out_len);
// data_json -> linha JSONL em out. Retorna tamanho ou negativo.
int nv_seal_vote(const uint8_t* data, size_t data_len, uint8_t* out, size_t out_len);
// 0 = assinatura válida.
int nv_verify(const uint8_t* pubkey32, const uint8_t* sig64, const uint8_t* msg, size_t msg_len);
// SHA-256: out precisa de >= 32. Retorna 0 ok.
int nv_sha256(const uint8_t* msg, size_t msg_len, uint8_t* out, size_t out_len);
// PHC Argon2id do PIN em out (NUL). out_len >= 64. Retorna tamanho ou negativo.
int nv_hash_pin(const uint8_t* pin, size_t pin_len, uint8_t* out, size_t out_len);
// 0 = PIN confere com o PHC (erro genérico, sem oráculo).
int nv_verify_pin(const uint8_t* pin, size_t pin_len, const uint8_t* phc, size_t phc_len);
}
