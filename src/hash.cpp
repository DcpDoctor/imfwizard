#include "imfwizard/hash.h"
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <fstream>
#include <vector>
#include <stdexcept>

namespace imfwizard {

std::string sha1_base64(const std::filesystem::path& file)
{
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("Cannot open file: " + file.string());

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        throw std::runtime_error("Failed to create EVP_MD_CTX");

    EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);

    std::vector<char> buf(65536);
    while (ifs.read(buf.data(), buf.size()) || ifs.gcount() > 0) {
        EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(ifs.gcount()));
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    // Base64 encode
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, hash, static_cast<int>(hash_len));
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);

    return result;
}

} // namespace imfwizard
