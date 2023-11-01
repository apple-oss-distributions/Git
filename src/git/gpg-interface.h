#ifndef GPG_INTERFACE_H
#define GPG_INTERFACE_H

struct strbuf;

#define GPG_VERIFY_VERBOSE		1
#define GPG_VERIFY_RAW			2
#define GPG_VERIFY_OMIT_STATUS	4

enum signature_trust_level {
	TRUST_UNDEFINED,
	TRUST_NEVER,
	TRUST_MARGINAL,
	TRUST_FULLY,
	TRUST_ULTIMATE,
};

struct signature_check {
	char *payload;
	char *gpg_output;
	char *gpg_status;

	/*
	 * possible "result":
	 * 0 (not checked)
	 * N (checked but no further result)
	 * G (good)
	 * B (bad)
	 */
	char result;
	char *signer;
	char *key;
	char *fingerprint;
	char *primary_key_fingerprint;
	enum signature_trust_level trust_level;
};

void signature_check_clear(struct signature_check *sigc);

/*
 * Look at a GPG signed tag object.  If such a signature exists, store it in
 * signature and the signed content in payload.  Return 1 if a signature was
 * found, and 0 otherwise.
 */
int parse_signature(const char *buf, size_t size, struct strbuf *payload, struct strbuf *signature);

/*
 * Look at GPG signed content (e.g. a signed tag object), whose
 * payload is followed by a detached signature on it.  Return the
 * offset where the embedded detached signature begins, or the end of
 * the data when there is no such signature.
 */
size_t parse_signed_buffer(const char *buf, size_t size);

/*
 * Create a detached signature for the contents of "buffer" and append
 * it after "signature"; "buffer" and "signature" can be the same
 * strbuf instance, which would cause the detached signature appended
 * at the end.
 */
int sign_buffer(struct strbuf *buffer, struct strbuf *signature,
		const char *signing_key);

int git_gpg_config(const char *, const char *, void *);
void set_signing_key(const char *);
const char *get_signing_key(void);
int check_signature(const char *payload, size_t plen,
		    const char *signature, size_t slen,
		    struct signature_check *sigc);
void print_signature_buffer(const struct signature_check *sigc,
			    unsigned flags);

#endif
