/*
 * AWS IoT Device SDK for Embedded C V202009.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef TEST_CONFIG_H_
#define TEST_CONFIG_H_

/**************************************************/
/******* DO NOT CHANGE the following order ********/
/**************************************************/

/* Logging config definition and header files inclusion are required in the following order:
 * 1. Include the header file "logging_levels.h".
 * 2. Define the LIBRARY_LOG_NAME and LIBRARY_LOG_LEVEL macros depending on
 * the logging configuration for DEMO.
 * 3. Include the header file "logging_stack.h", if logging is enabled for DEMO.
 */

/* Include header that defines log levels. */
#include "logging_levels.h"

/* Logging configuration for the Demo. */
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "TEST"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif

#include "logging_stack.h"

/************ End of logging configuration ****************/

/**
 * @brief HTTP server host name.
 */
#define SERVER_HOST    "httpbin.org"

/**
 * @brief HTTP server port number.
 *
 * In general, port 443 is for TLS HTTP connections.
 */
#define HTTPS_PORT     443

/**
 * @brief Path of the file containing the server's root CA certificate for TLS authentication.
 *
 * #define ROOT_CA_CERT_PATH    "...insert here..."
 */

/**
 * @brief Path of the file containing the client's certificate for TLS
 * authentication to AWS IoT Core.
 *
 * @note This certificate should be PEM-encoded and must have an associated
 * policy from AWS IoT core for the demo to function correctly.
 *
 * #define CLIENT_CERT_PATH                  "...insert here..."
 */

/**
 * @brief Path of the file containing the client's private key for
 * TLS client authentication.
 *
 * @note This key should be PEM-encoded and must have an associated
 * policy from AWS IoT core for the demo to function correctly.
 *
 * #define CLIENT_PRIVATE_KEY_PATH           "...insert here..."
 */

/**
 * @brief ALPN protocol name to be sent as part of the message sent to AWS IoT Core.
 *
 * @note Only define when using AWS IoT Core. When using ALPN, port 443 must be used.
 *
 * #define IOT_CORE_ALPN_PROTOCOL_NAME       "\x0ex-amzn-http-ca"
 */
#define IOT_CORE_ALPN_PROTOCOL_NAME       "\x0ex-amzn-http-ca"

/**
 * @brief Paths for different HTTP methods for specified host.
 */
#define GET_PATH                          "/get"
#define HEAD_PATH                         "/get"
#define PUT_PATH                          "/put"
#define POST_PATH                         "/post"

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define TRANSPORT_SEND_RECV_TIMEOUT_MS    ( 5000 )

/**
 * @brief The length in bytes of the user buffer.
 */
#define USER_BUFFER_LENGTH                ( 2048 )

/**
 * @brief Request body to send for PUT and POST requests.
 */
#define REQUEST_BODY                      "Hello, world!"

#endif /* ifndef DEMO_CONFIG_H_ */
