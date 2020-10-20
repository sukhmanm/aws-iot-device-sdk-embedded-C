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

/* Standard includes. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* POSIX includes. */
#include <unistd.h>

/* Include Demo Config as the first non-system header. */
#include "demo_config.h"

/* Common HTTP demo utilities. */
#include "http_demo_utils.h"

/* HTTP API header. */
#include "core_http_client.h"

/* OpenSSL transport header. */
#include "openssl_posix.h"

/* Retry utilities. */
#include "retry_utils.h"

/* Check that TLS port of the server is defined. */
#ifndef HTTPS_PORT
    #error "Please define a HTTPS_PORT."
#endif

/* Check that a path for Root CA Certificate is defined. */
#ifndef ROOT_CA_CERT_PATH
    #error "Please define a ROOT_CA_CERT_PATH."
#endif

/* Check that the pre-signed PUT URL is defined. */
#ifndef S3_PRESIGNED_PUT_URL
    #error "Please define a S3_PRESIGNED_PUT_URL."
#endif

/* Check that the pre-signed GET URL is defined. */
#ifndef S3_PRESIGNED_GET_URL
    #error "Please define a S3_PRESIGNED_GET_URL."
#endif

/* Check that transport timeout for transport send and receive is defined. */
#ifndef TRANSPORT_SEND_RECV_TIMEOUT_MS
    #define TRANSPORT_SEND_RECV_TIMEOUT_MS    ( 1000 )
#endif

/* Check that size of the user buffer is defined. */
#ifndef USER_BUFFER_LENGTH
    #define USER_BUFFER_LENGTH    ( 2048 )
#endif

/* Pointer to the data to upload.*/
#ifndef DEMO_HTTP_UPLOAD_DATA
    #define DEMO_HTTP_UPLOAD_DATA    "Hello World!"
#endif

/**
 * @brief Length of the pre-signed PUT URL defined in demo_config.h.
 */
#define S3_PRESIGNED_PUT_URL_LENGTH               ( sizeof( S3_PRESIGNED_PUT_URL ) - 1 )

/**
 * @brief Length of the pre-signed GET URL defined in demo_config.h.
 */
#define S3_PRESIGNED_GET_URL_LENGTH               ( sizeof( S3_PRESIGNED_GET_URL ) - 1 )

/**
 * @brief ALPN protocol name to be sent as part of the ClientHello message.
 *
 * @note When using ALPN, port 443 must be used to connect to AWS IoT Core.
 */
#define IOT_CORE_ALPN_PROTOCOL_NAME               "\x0ex-amzn-http-ca"

/**
 * @brief Length of ALPN protocol name to be sent as part of the ClientHello
 * message.
 */
#define IOT_CORE_ALPN_PROTOCOL_NAME_LENGTH        ( sizeof( IOT_CORE_ALPN_PROTOCOL_NAME ) - 1 )

/**
 * @brief Field name of the HTTP Range header to read from server response.
 */
#define HTTP_CONTENT_RANGE_HEADER_FIELD           "Content-Range"

/**
 * @brief Length of the HTTP Range header field.
 */
#define HTTP_CONTENT_RANGE_HEADER_FIELD_LENGTH    ( sizeof( HTTP_CONTENT_RANGE_HEADER_FIELD ) - 1 )

/**
 * @brief Delay in seconds between each iteration of the demo.
 */
#define DEMO_LOOP_DELAY_SECONDS                   ( 5U )

/**
 * @brief The length of the data in bytes to upload.
 */
#define DEMO_HTTP_UPLOAD_DATA_LENGTH              ( sizeof( DEMO_HTTP_UPLOAD_DATA ) - 1 )

/**
 * @brief The length of the HTTP GET method.
 */
#define HTTP_METHOD_GET_LENGTH                    ( sizeof( HTTP_METHOD_GET ) - 1 )

/**
 * @brief The length of the HTTP PUT method.
 */
#define HTTP_METHOD_PUT_LENGTH                    ( sizeof( HTTP_METHOD_PUT ) - 1 )

/**
 * @brief A buffer used in the demo for storing HTTP request headers and HTTP
 * response headers and body.
 *
 * @note This demo shows how the same buffer can be re-used for storing the HTTP
 * response after the HTTP request is sent out. However, the user can also
 * decide to use separate buffers for storing the HTTP request and response.
 */
static uint8_t userBuffer[ USER_BUFFER_LENGTH ];

/**
 * @brief Represents header data that will be sent in an HTTP request.
 */
static HTTPRequestHeaders_t requestHeaders;

/**
 * @brief Configurations of the initial request headers that are passed to
 * #HTTPClient_InitializeRequestHeaders.
 */
static HTTPRequestInfo_t requestInfo;

/**
 * @brief Represents a response returned from an HTTP server.
 */
static HTTPResponse_t response;

/**
 * @brief The host address string extracted from the pre-signed URL.
 *
 * @note S3_PRESIGNED_PUT_URL_LENGTH is set as the array length here as the
 * length of the host name string cannot exceed this value.
 */
static char serverHost[ S3_PRESIGNED_PUT_URL_LENGTH ];

/**
 * @brief The length of the host address found in the pre-signed URL.
 */
static size_t serverHostLength;

/**
 * @brief The location of the path within the pre-signed URL.
 */
static const char * pPath;

/*-----------------------------------------------------------*/

/**
 * @brief Connect to HTTP server with reconnection retries.
 *
 * @param[out] pNetworkContext The output parameter to return the created
 * network context.
 *
 * @return EXIT_FAILURE on failure; EXIT_SUCCESS on successful connection.
 */
static int32_t connectToServer( NetworkContext_t * pNetworkContext );

/**
 * @brief Retrieve and verify the size of the S3 object that is specified in
 * pPath.
 *
 * @param[out] pFileSize The size of the S3 object.
 * @param[in] pTransportInterface The transport interface for making network
 * calls.
 * @param[in] pPath The Request-URI to the objects of interest.
 */
static HTTPStatus_t verifyS3ObjectFileSize( size_t * pFileSize,
                                            const TransportInterface_t * pTransportInterface,
                                            const char * pPath );

/**
 * @brief Send an HTTP PUT request based on a specified path to upload a file,
 * then print the response received from the server.
 *
 * @param[in] pTransportInterface The transport interface for making network
 * calls.
 * @param[in] pPath The Request-URI to the objects of interest.
 */
static HTTPStatus_t uploadS3ObjectFile( const TransportInterface_t * pTransportInterface,
                                        const char * pPath );

/*-----------------------------------------------------------*/

static int32_t connectToServer( NetworkContext_t * pNetworkContext )
{
    int32_t returnStatus = EXIT_FAILURE;
    HTTPStatus_t httpStatus = HTTP_SUCCESS;

    /* The location of the host address within the pre-signed URL. */
    const char * pAddress = NULL;

    /* Status returned by OpenSSL transport implementation. */
    OpensslStatus_t opensslStatus;
    /* Credentials to establish the TLS connection. */
    OpensslCredentials_t opensslCredentials;
    /* Information about the server to send the HTTP requests. */
    ServerInfo_t serverInfo;

    /* Initialize TLS credentials. */
    ( void ) memset( &opensslCredentials, 0, sizeof( opensslCredentials ) );
    opensslCredentials.pRootCaPath = ROOT_CA_CERT_PATH;

    /* ALPN is required when communicating to AWS IoT Core over port 443 through
     * HTTP. */
    if( HTTPS_PORT == 443 )
    {
        opensslCredentials.pAlpnProtos = IOT_CORE_ALPN_PROTOCOL_NAME;
        opensslCredentials.alpnProtosLen = IOT_CORE_ALPN_PROTOCOL_NAME_LENGTH;
    }

    /* Retrieve the address location and length from S3_PRESIGNED_PUT_URL. */
    httpStatus = getUrlAddress( S3_PRESIGNED_PUT_URL,
                                S3_PRESIGNED_PUT_URL_LENGTH,
                                &pAddress,
                                &serverHostLength );

    returnStatus = ( httpStatus == HTTP_SUCCESS ) ? EXIT_SUCCESS : EXIT_FAILURE;

    if( returnStatus == EXIT_SUCCESS )
    {
        /* serverHost should consist only of the host address located in
         * S3_PRESIGNED_PUT_URL. */
        memcpy( serverHost, pAddress, serverHostLength );
        serverHost[ serverHostLength ] = '\0';

        /* Initialize server information. */
        serverInfo.pHostName = serverHost;
        serverInfo.hostNameLength = serverHostLength;
        serverInfo.port = HTTPS_PORT;

        /* Establish a TLS session with the HTTP server. This example connects
         * to the HTTP server as specified in SERVER_HOST and HTTPS_PORT in
         * demo_config.h. */
        LogInfo( ( "Establishing a TLS session with %s:%d.",
                   serverHost,
                   HTTPS_PORT ) );

        opensslStatus = Openssl_Connect( pNetworkContext,
                                         &serverInfo,
                                         &opensslCredentials,
                                         TRANSPORT_SEND_RECV_TIMEOUT_MS,
                                         TRANSPORT_SEND_RECV_TIMEOUT_MS );

        returnStatus = ( opensslStatus == OPENSSL_SUCCESS ) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t verifyS3ObjectFileSize( size_t * pFileSize,
                                            const TransportInterface_t * pTransportInterface,
                                            const char * pPath )
{
    HTTPStatus_t httpStatus = HTTP_SUCCESS;

    /* The location of the file size in contentRangeValStr. */
    char * pFileSizeStr = NULL;

    /* String to store the Content-Range header value. */
    char * contentRangeValStr = NULL;
    size_t contentRangeValStrLength = 0;

    /* Initialize the request object. */
    requestInfo.pHost = serverHost;
    requestInfo.hostLen = serverHostLength;
    requestInfo.method = HTTP_METHOD_GET;
    requestInfo.methodLen = HTTP_METHOD_GET_LENGTH;
    requestInfo.pPath = pPath;
    requestInfo.pathLen = strlen( pPath );

    /* Set "Connection" HTTP header to "keep-alive" so that multiple requests
     * can be sent over the same established TCP connection. */
    requestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

    /* Set the buffer used for storing request headers. */
    requestHeaders.pBuffer = userBuffer;
    requestHeaders.bufferLen = USER_BUFFER_LENGTH;

    /* Initialize the response object. The same buffer used for storing request
     * headers is reused here. */
    response.pBuffer = userBuffer;
    response.bufferLen = USER_BUFFER_LENGTH;

    LogInfo( ( "Getting file object size from host..." ) );

    httpStatus = HTTPClient_InitializeRequestHeaders( &requestHeaders,
                                                      &requestInfo );

    if( httpStatus == HTTP_SUCCESS )
    {
        /* Add the header to get bytes=0-0. S3 will respond with a Content-Range
         * header that contains the size of the file in it. This header will
         * look like: "Content-Range: bytes 0-0/FILESIZE". The body will have a
         * single byte that we are ignoring. */
        httpStatus = HTTPClient_AddRangeHeader( &requestHeaders, 0, 0 );
    }

    if( httpStatus == HTTP_SUCCESS )
    {
        /* Send the request and receive the response. */
        httpStatus = HTTPClient_Send( pTransportInterface,
                                      &requestHeaders,
                                      NULL,
                                      0,
                                      &response,
                                      0 );
    }

    if( httpStatus == HTTP_SUCCESS )
    {
        LogInfo( ( "Received HTTP response from %s%s...",
                   serverHost, pPath ) );
        LogInfo( ( "Response Headers:\n%.*s",
                   ( int32_t ) response.headersLen,
                   response.pHeaders ) );
        LogInfo( ( "Response Status:\n%u",
                   response.statusCode ) );
        LogInfo( ( "Response Body:\n%.*s\n",
                   ( int32_t ) response.bodyLen,
                   response.pBody ) );

        httpStatus = HTTPClient_ReadHeader( &response,
                                            ( char * ) HTTP_CONTENT_RANGE_HEADER_FIELD,
                                            ( size_t ) HTTP_CONTENT_RANGE_HEADER_FIELD_LENGTH,
                                            ( const char ** ) &contentRangeValStr,
                                            &contentRangeValStrLength );
    }
    else
    {
        LogError( ( "Failed to send HTTP GET request to %s%s: Error=%s.",
                    serverHost, pPath, HTTPClient_strerror( httpStatus ) ) );
    }

    if( httpStatus == HTTP_SUCCESS )
    {
        /* Parse the Content-Range header value to get the file size. */
        pFileSizeStr = strstr( contentRangeValStr, "/" );

        if( pFileSizeStr == NULL )
        {
            LogError( ( "'/' not present in Content-Range header value: %s.",
                        contentRangeValStr ) );
        }

        pFileSizeStr += sizeof( char );
        *pFileSize = ( size_t ) strtoul( pFileSizeStr, NULL, 10 );

        if( ( *pFileSize == 0 ) || ( *pFileSize == UINT32_MAX ) )
        {
            LogError( ( "Error using strtoul to get the file size from %s: fileSize=%d.",
                        pFileSizeStr, ( int32_t ) *pFileSize ) );
            httpStatus = HTTP_INVALID_PARAMETER;
        }

        LogInfo( ( "The file is %d bytes long.", ( int32_t ) *pFileSize ) );
    }
    else
    {
        LogError( ( "Failed to read Content-Range header from HTTP response: Error=%s.",
                    HTTPClient_strerror( httpStatus ) ) );
    }

    if( httpStatus == HTTP_SUCCESS )
    {
        if( *pFileSize != DEMO_HTTP_UPLOAD_DATA_LENGTH )
        {
            LogError( ( "Failed to upload the data to S3. The file size found is %d, but it should be %d.",
                        ( int32_t ) *pFileSize,
                        ( int32_t ) DEMO_HTTP_UPLOAD_DATA_LENGTH ) );
        }
        else
        {
            LogInfo( ( "Successfuly verified that the size of the file found on S3 matches the file size uploaded "
                       "(Uploaded: %d bytes, Found: %d bytes).",
                       ( int32_t ) DEMO_HTTP_UPLOAD_DATA_LENGTH,
                       ( int32_t ) *pFileSize ) );
        }
    }
    else
    {
        LogError( ( "An error occurred in getting the file size from %s. Error=%s.",
                    serverHost,
                    HTTPClient_strerror( httpStatus ) ) );
    }

    return httpStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t uploadS3ObjectFile( const TransportInterface_t * pTransportInterface,
                                        const char * pPath )
{
    HTTPStatus_t httpStatus = HTTP_SUCCESS;

    /* Initialize the request object. */
    requestInfo.pHost = serverHost;
    requestInfo.hostLen = serverHostLength;
    requestInfo.method = HTTP_METHOD_PUT;
    requestInfo.methodLen = HTTP_METHOD_PUT_LENGTH;
    requestInfo.pPath = pPath;
    requestInfo.pathLen = strlen( pPath );

    /* Set "Connection" HTTP header to "keep-alive" so that multiple requests
     * can be sent over the same established TCP connection. */
    requestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

    /* Set the buffer used for storing request headers. */
    requestHeaders.pBuffer = userBuffer;
    requestHeaders.bufferLen = USER_BUFFER_LENGTH;

    /* Initialize the response object. The same buffer used for storing request
     * headers is reused here. */
    response.pBuffer = userBuffer;
    response.bufferLen = USER_BUFFER_LENGTH;

    if( httpStatus == HTTP_SUCCESS )
    {
        httpStatus = HTTPClient_InitializeRequestHeaders( &requestHeaders,
                                                          &requestInfo );
    }

    if( httpStatus == HTTP_SUCCESS )
    {
        LogInfo( ( "Uploading file..." ) );
        LogInfo( ( "Request Headers:\n%.*s",
                   ( int32_t ) requestHeaders.headersLen,
                   ( char * ) requestHeaders.pBuffer ) );
        httpStatus = HTTPClient_Send( pTransportInterface,
                                      &requestHeaders,
                                      ( const uint8_t * ) DEMO_HTTP_UPLOAD_DATA,
                                      DEMO_HTTP_UPLOAD_DATA_LENGTH,
                                      &response,
                                      0 );
    }
    else
    {
        LogError( ( "Failed to initialize HTTP request headers: Error=%s.",
                    HTTPClient_strerror( httpStatus ) ) );
    }

    if( httpStatus == HTTP_SUCCESS )
    {
        LogInfo( ( "Received HTTP response from %s%s...",
                   serverHost, pPath ) );
        LogInfo( ( "Response Headers:\n%.*s",
                   ( int32_t ) response.headersLen,
                   response.pHeaders ) );
        LogInfo( ( "Response Status:\n%u",
                   response.statusCode ) );
        LogInfo( ( "Response Body:\n%.*s\n",
                   ( int32_t ) response.bodyLen,
                   response.pBody ) );
    }
    else
    {
        LogError( ( "An error occurred in uploading the file."
                    "Failed to send HTTP PUT request to %s%s: Error=%s.",
                    serverHost, pPath, HTTPClient_strerror( httpStatus ) ) );
    }

    return httpStatus;
}

/*-----------------------------------------------------------*/

/**
 * @brief Entry point of demo.
 *
 * This example, using a pre-signed URL, resolves a S3 domain, establishes a TCP
 * connection, validates the server's certificate using the root CA certificate
 * defined in the config header, and then finally performs a TLS handshake with
 * the HTTP server so that all communication is encrypted. After which, the HTTP
 * Client library API is used to upload a file to a S3 bucket by sending a PUT
 * request, and verify the file was uploaded using a GET request. If any request
 * fails, an error code is returned.
 *
 * @note This example is single-threaded and uses statically allocated memory.
 *
 */
int main( int argc,
          char ** argv )
{
    /* Return value of main. */
    int32_t returnStatus = EXIT_SUCCESS;
    /* HTTPS Client library return status. */
    HTTPStatus_t httpStatus = HTTP_SUCCESS;
    /* The length of the path within the pre-signed URL. This variable is defined
     * in order to store the length returned from parsing the URL, but it is
     * unused. The path used for the requests in this demo needs all the query
     * information following the location of the object, to the end of the S3 presigned
     * URL. */
    size_t pathLen = 0;
    /* The size of the file uploaded to S3. */
    size_t fileSize = 0;

    /* The transport layer interface used by the HTTP Client library. */
    TransportInterface_t transportInterface;
    /* The network context for the transport layer interface. */
    NetworkContext_t networkContext;

    ( void ) argc;
    ( void ) argv;

    for( ; ; )
    {
        LogInfo( ( "HTTPS Client Synchronous S3 upload demo using pre-signed PUT URL:\n%s", S3_PRESIGNED_PUT_URL ) );

        /************************* Parse Signed URL. *************************/
        if( returnStatus == EXIT_SUCCESS )
        {
            /* Retrieve the path location from S3_PRESIGNED_PUT_URL. This
             * function returns the length of the path without the query into
             * pathLen. */
            httpStatus = getUrlPath( S3_PRESIGNED_PUT_URL,
                                     S3_PRESIGNED_PUT_URL_LENGTH,
                                     &pPath,
                                     &pathLen );

            returnStatus = ( httpStatus == HTTP_SUCCESS ) ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        /**************************** Connect. ******************************/

        /* Establish TLS connection on top of TCP connection using OpenSSL. */
        if( returnStatus == EXIT_SUCCESS )
        {
            /* Attempt to connect to the HTTP server. If connection fails, retry
             * after a timeout. Timeout value will be exponentially increased
             * till the maximum attempts are reached or maximum timeout value is
             * reached. The function returns EXIT_FAILURE if the TCP connection
             * cannot be established to broker after configured number of
             * attempts. */
            returnStatus = connectToServerWithBackoffRetries( connectToServer,
                                                              &networkContext );

            if( returnStatus == EXIT_FAILURE )
            {
                /* Log error to indicate connection failure after all reconnect
                 * attempts are over. */
                LogError( ( "Failed to connect to HTTP server %s.",
                            serverHost ) );
            }
        }

        /* Define the transport interface. */
        if( returnStatus == EXIT_SUCCESS )
        {
            ( void ) memset( &transportInterface, 0, sizeof( transportInterface ) );
            transportInterface.recv = Openssl_Recv;
            transportInterface.send = Openssl_Send;
            transportInterface.pNetworkContext = &networkContext;
        }

        /********************** Upload S3 Object File. **********************/

        if( returnStatus == EXIT_SUCCESS )
        {
            httpStatus = uploadS3ObjectFile( &transportInterface,
                                             pPath );

            returnStatus = ( httpStatus == HTTP_SUCCESS ) ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        /******************* Verify S3 Object File Upload. ********************/

        if( returnStatus == EXIT_SUCCESS )
        {
            /* Retrieve the path location from S3_PRESIGNED_GET_URL. This
             * function returns the length of the path without the query into
             * pathLen. */
            httpStatus = getUrlPath( S3_PRESIGNED_GET_URL,
                                     S3_PRESIGNED_GET_URL_LENGTH,
                                     &pPath,
                                     &pathLen );

            returnStatus = ( httpStatus == HTTP_SUCCESS ) ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        if( returnStatus == EXIT_SUCCESS )
        {
            /* Verify the file exists by retrieving the file size. */
            httpStatus = verifyS3ObjectFileSize( &fileSize,
                                                 &transportInterface,
                                                 pPath );

            returnStatus = ( httpStatus == HTTP_SUCCESS ) ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        if( returnStatus == EXIT_SUCCESS )
        {
            /* Log a message indicating an iteration completed successfully. */
            LogInfo( ( "Demo completed successfully." ) );
        }

        /************************** Disconnect. *****************************/

        /* End the TLS session, then close the TCP connection. */
        ( void ) Openssl_Disconnect( &networkContext );

        LogInfo( ( "Short delay before starting the next iteration....\n" ) );
        sleep( DEMO_LOOP_DELAY_SECONDS );
    }

    return returnStatus;
}