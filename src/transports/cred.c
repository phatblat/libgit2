/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"
#include "smart.h"
#include "git2/cred_helpers.h"

static void plaintext_free(struct git_cred *cred)
{
	git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;

	git__free(c->username);

	/* Zero the memory which previously held the password */
	if (c->password) {
		size_t pass_len = strlen(c->password);
		git__memzero(c->password, pass_len);
		git__free(c->password);
	}

	git__memzero(c, sizeof(*c));
	git__free(c);
}

int git_cred_userpass_plaintext_new(
	git_cred **cred,
	const char *username,
	const char *password)
{
	git_cred_userpass_plaintext *c;

	assert(cred);

	c = git__malloc(sizeof(git_cred_userpass_plaintext));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_USERPASS_PLAINTEXT;
	c->parent.free = plaintext_free;
	c->username = git__strdup(username);

	if (!c->username) {
		git__free(c);
		return -1;
	}

	c->password = git__strdup(password);

	if (!c->password) {
		git__free(c->username);
		git__free(c);
		return -1;
	}

	*cred = &c->parent;
	return 0;
}

static void ssh_keyfile_passphrase_free(struct git_cred *cred)
{
	git_cred_ssh_keyfile_passphrase *c =
		(git_cred_ssh_keyfile_passphrase *)cred;

	git__free(c->publickey);
	git__free(c->privatekey);

	if (c->passphrase) {
		/* Zero the memory which previously held the passphrase */
		size_t pass_len = strlen(c->passphrase);
		git__memzero(c->passphrase, pass_len);
		git__free(c->passphrase);
	}

	git__memzero(c, sizeof(*c));
	git__free(c);
}

static void ssh_publickey_free(struct git_cred *cred)
{
	git_cred_ssh_publickey *c = (git_cred_ssh_publickey *)cred;

	git__free(c->publickey);

	git__memzero(c, sizeof(*c));
	git__free(c);
}

int git_cred_ssh_keyfile_passphrase_new(
	git_cred **cred,
	const char *publickey,
	const char *privatekey,
	const char *passphrase)
{
	git_cred_ssh_keyfile_passphrase *c;

	assert(cred && privatekey);

	c = git__calloc(1, sizeof(git_cred_ssh_keyfile_passphrase));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_KEYFILE_PASSPHRASE;
	c->parent.free = ssh_keyfile_passphrase_free;

	c->privatekey = git__strdup(privatekey);
	GITERR_CHECK_ALLOC(c->privatekey);

	if (publickey) {
		c->publickey = git__strdup(publickey);
		GITERR_CHECK_ALLOC(c->publickey);
	}

	if (passphrase) {
		c->passphrase = git__strdup(passphrase);
		GITERR_CHECK_ALLOC(c->passphrase);
	}

	*cred = &c->parent;
	return 0;
}

int git_cred_ssh_publickey_new(
	git_cred **cred,
	const char *publickey,
	size_t publickey_len,
	git_cred_sign_callback sign_callback,
	void *sign_data)
{
	git_cred_ssh_publickey *c;

	assert(cred);

	c = git__calloc(1, sizeof(git_cred_ssh_publickey));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_PUBLICKEY;
	c->parent.free = ssh_publickey_free;

	if (publickey_len > 0) {
		c->publickey = git__malloc(publickey_len);
		GITERR_CHECK_ALLOC(c->publickey);

		memcpy(c->publickey, publickey, publickey_len);
	}

	c->publickey_len = publickey_len;
	c->sign_callback = sign_callback;
	c->sign_data = sign_data;

	*cred = &c->parent;
	return 0;
}
