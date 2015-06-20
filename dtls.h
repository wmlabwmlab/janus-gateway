/*! \file    dtls.h
 * \author   Lorenzo Miniero <lorenzo@meetecho.com>
 * \copyright GNU General Public License v3
 * \brief    DTLS/SRTP processing (headers)
 * \details  Implementation (based on OpenSSL and libsrtp) of the DTLS/SRTP
 * transport. The code takes care of the DTLS handshake between peers and
 * the gateway, and sets the proper SRTP and SRTCP context up accordingly.
 * A DTLS alert from a peer is notified to the plugin handling him/her
 * by means of the hangup_media callback.
 * 
 * \ingroup protocols
 * \ref protocols
 */
 
#ifndef _JANUS_DTLS_H
#define _JANUS_DTLS_H

#include <inttypes.h>
#include <glib.h>
#include <srtp/srtp.h>

#include "sctp.h"
#include "dtls-bio.h"

/*! \brief DTLS stuff initialization
 * @param[in] server_pem Path to the certificate to use
 * @param[in] server_key Path to the key to use
 * @returns 0 in case of success, a negative integer on errors */
gint janus_dtls_srtp_init(gchar *server_pem, gchar *server_key);
/*! \brief Method to return the shared SSL_CTX instance */
SSL_CTX *janus_dtls_get_ssl_ctx(void);
/*! \brief Method to return a string representation (SHA-256) of the certificate fingerprint */
gchar *janus_dtls_get_local_fingerprint(void);


/*! \brief DTLS roles */
typedef enum janus_dtls_role {
	JANUS_DTLS_ROLE_ACTPASS = -1,
	JANUS_DTLS_ROLE_SERVER,
	JANUS_DTLS_ROLE_CLIENT,
} janus_dtls_role;

/*! \brief DTLS state */
typedef enum janus_dtls_state {
	JANUS_DTLS_STATE_FAILED = -1,
	JANUS_DTLS_STATE_CREATED,
	JANUS_DTLS_STATE_TRYING,
	JANUS_DTLS_STATE_CONNECTED,
} janus_dtls_state;

/*! \brief Janus DTLS-SRTP handle */
typedef struct janus_dtls_srtp {
	/*! \brief Opaque pointer to the component this DTLS-SRTP context belongs to */
	void *component;
	/*! \brief DTLS role of the gateway for this stream: 1=client, 0=server */
	janus_dtls_role dtls_role;
	/*! \brief DTLS state of this component: -1=failed, 0=nothing, 1=trying, 2=connected */
	janus_dtls_state dtls_state;
	/*! \brief SSL context used for DTLS for this component */
	SSL *ssl;
	/*! \brief Read BIO (incoming DTLS data) */
	BIO *read_bio;
	/*! \brief Write BIO (outgoing DTLS data) */
	BIO *write_bio;
	/*! \brief Filter BIO (fix MTU fragmentation on outgoing DTLS data, if required) */
	BIO *filter_bio;
	/*! \brief Whether SRTP has been correctly set up for this component or not */
	gint srtp_valid;
	/*! \brief libsrtp context for incoming SRTP packets */
	srtp_t srtp_in;
	/*! \brief libsrtp context for outgoing SRTP packets */
	srtp_t srtp_out;
	/*! \brief libsrtp policy for incoming SRTP packets */
	srtp_policy_t remote_policy;
	/*! \brief libsrtp policy for outgoing SRTP packets */
	srtp_policy_t local_policy;
	/*! \brief Mutex to lock/unlock this libsrtp context */
	janus_mutex srtp_mutex;
	/*! \brief Whether this DTLS stack is now ready to be used for messages as well (e.g., SCTP encapsulation) */
	int ready;
#ifdef HAVE_SCTP
	/*! \brief SCTP association, if DataChannels are involved */
	janus_sctp_association *sctp;
#endif
} janus_dtls_srtp;


/*! \brief Create a janus_dtls_srtp instance
 * @param[in] component Opaque pointer to the component owning that will use the stack
 * @param[in] role The role of the DTLS stack (client/server)
 * @returns A new janus_dtls_srtp instance if successful, NULL otherwise */
janus_dtls_srtp *janus_dtls_srtp_create(void *component, janus_dtls_role role);
/*! \brief Start a DTLS handshake
 * @param[in] dtls The janus_dtls_srtp instance to start the handshake on */
void janus_dtls_srtp_handshake(janus_dtls_srtp *dtls);
/*! \brief Handle an incoming DTLS message
 * @param[in] dtls The janus_dtls_srtp instance to start the handshake on
 * @param[in] buf The DTLS message data
 * @param[in] len The DTLS message data lenght */
void janus_dtls_srtp_incoming_msg(janus_dtls_srtp *dtls, char *buf, uint16_t len);
/*! \brief Send an alert on a janus_dtls_srtp instance
 * @param[in] dtls The janus_dtls_srtp instance to send the alert on */
void janus_dtls_srtp_send_alert(janus_dtls_srtp *dtls);
/*! \brief Destroy a janus_dtls_srtp instance
 * @param[in] dtls The janus_dtls_srtp instance to destroy */
void janus_dtls_srtp_destroy(janus_dtls_srtp *dtls);

/*! \brief DTLS alert callback (http://www.openssl.org/docs/ssl/SSL_CTX_set_info_callback.html)
 * @param[in] ssl SSL instance where the alert occurred
 * @param[in] where The context where the event occurred
 * @param[in] ret The error code */
void janus_dtls_callback(const SSL *ssl, int where, int ret);

/*! \brief DTLS certificate verification callback (http://www.openssl.org/docs/ssl/SSL_CTX_set_verify.html)
 * \details This method always returns 1 (true), in order not to fail when a certificate verification is requested. This is especially needed because all certificates used for DTLS in WebRTC are self signed, and as such a formal verification would fail.
 * @param[in] preverify_ok Whether the verification of the certificate was passed
 * @param[in] ctx context used for the certificate verification */
int janus_dtls_verify_callback(int preverify_ok, X509_STORE_CTX *ctx);

/*! \brief DTLS BIOs to/from socket bridge
 * \details As libnice is going to actually send and receive data, and not OpenSSL, a read/write BIO is used to "bridge" the data between the crypto stuff and the network.
 * @param[in] dtls The janus_dtls_srtp instance to use */
void janus_dtls_fd_bridge(janus_dtls_srtp *dtls);

#ifdef HAVE_SCTP
/*! \brief Callback (called from the ICE handle) to encapsulate in DTLS outgoing SCTP data (DataChannel)
 * @param[in] dtls The janus_dtls_srtp instance to use
 * @param[in] buf The data buffer to encapsulate
 * @param[in] len The data length */
void janus_dtls_wrap_sctp_data(janus_dtls_srtp *dtls, char *buf, int len);

/*! \brief Callback (called from the SCTP stack) to encapsulate in DTLS outgoing SCTP data (DataChannel)
 * @param[in] dtls The janus_dtls_srtp instance to use
 * @param[in] buf The data buffer to encapsulate
 * @param[in] len The data length
 * @returns The number of sent bytes in case of success, 0 or a negative integer otherwise */
int janus_dtls_send_sctp_data(janus_dtls_srtp *dtls, char *buf, int len);

/*! \brief Callback to be notified about incoming SCTP data (DataChannel) to forward to the handle
 * @param[in] dtls The janus_dtls_srtp instance to use
 * @param[in] buf The data buffer
 * @param[in] len The data length */
void janus_dtls_notify_data(janus_dtls_srtp *dtls, char *buf, int len);
#endif

/*! \brief DTLS retransmission timer
 * \details As libnice is going to actually send and receive data, OpenSSL cannot handle retransmissions by itself: this timed callback (g_source_set_callback) deals with this.
 * @param[in] stack Opaque pointer to the janus_dtls_srtp instance to use
 * @returns true if a retransmission is still needed, false otherwise */
gboolean janus_dtls_retry(gpointer stack);


/*! \brief Helper method to get a string representation of a libsrtp error code
 * @param[in] error The libsrtp error code
 * @returns A string representation of the error code */
const gchar *janus_get_srtp_error(int error);

/*! \brief Helper method to get a string representation of a Janus DTLS state
 * @param[in] state The Janus DTLS state
 * @returns A string representation of the state */
const gchar *janus_get_dtls_srtp_state(janus_dtls_state state);

/*! \brief Helper method to get a string representation of a DTLS role
 * @param[in] role The DTLS role
 * @returns A string representation of the role */
const gchar *janus_get_dtls_srtp_role(janus_dtls_role role);


#endif
