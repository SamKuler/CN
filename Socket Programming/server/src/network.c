/**
 * @file network.c
 * @brief Implementation of the cross-platform networking library.
 *
 * @version 0.1
 * @date 2025-10-19
 */
#include "network.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

int net_init(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return -1;
    }
#endif
    return 0;
}

void net_shutdown(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

socket_t net_create_listening_socket(net_addr_family_t family, uint16_t port, int backlog)
{
    struct addrinfo hints, *res, *p;
    socket_t listening_socket = INVALID_SOCKET_T;
    int opt = 1;
    char port_str[6]; // max "65535" + null terminator

    snprintf(port_str, sizeof(port_str), "%u", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // For wildcard IP address

    // Determine the ip family
    switch (family)
    {
    case NET_AF_IPV4:
        hints.ai_family = AF_INET;
        break;
    case NET_AF_IPV6:
        hints.ai_family = AF_INET6;
        break;
    case NET_AF_UNSPEC:
    default:
        hints.ai_family = AF_UNSPEC;
        break;
    }

    if (getaddrinfo(NULL, port_str, &hints, &res) != 0)
    {
        return INVALID_SOCKET_T;
    }

    // Access all available addresses
    for (p = res; p != NULL; p = p->ai_next)
    {
        listening_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listening_socket == INVALID_SOCKET_T)
        {
            continue;
        }

        // Allow reusing the address to avoid "Address already in use" errors on restart
        if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0)
        {
            net_close_socket(listening_socket);
            listening_socket = INVALID_SOCKET_T;
            continue;
        }

        // For IPv6, ensure it's not an IPv4-mapped address (IPv6 only)s
        if (p->ai_family == AF_INET6)
        {
            int ipv6_only = 1;
            if (setsockopt(listening_socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&ipv6_only, sizeof(ipv6_only)) < 0)
            {
                // This might fail on older systems, but we can often ignore the error
                // Here keep the if statement for unified structure
            }
        }

        if (bind(listening_socket, p->ai_addr, (int)p->ai_addrlen) < 0)
        {
            net_close_socket(listening_socket);
            listening_socket = INVALID_SOCKET_T;
            continue;
        }

        // If we got here, we successfully bound the socket. No need to find further.
        break;
    }

    freeaddrinfo(res);

    if (listening_socket == INVALID_SOCKET_T)
    {
        return INVALID_SOCKET_T; // Failed to bind to any address
    }

    if (listen(listening_socket, backlog) < 0)
    {
        net_close_socket(listening_socket);
        return INVALID_SOCKET_T;
    }

    return listening_socket;
}

socket_t net_accept_client(socket_t listening_socket, char *client_ip_buffer, size_t buffer_size, uint16_t *client_port)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    socket_t client_socket;

    client_socket = accept(listening_socket, (struct sockaddr *)&client_addr, &addr_len);

    if (client_socket == INVALID_SOCKET_T)
    {
        if (client_port)
            *client_port = 0;
        return INVALID_SOCKET_T;
    }

    if (client_addr.ss_family == AF_INET) // IPv4
    {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        if (client_ip_buffer && buffer_size > 0)
        {
            inet_ntop(AF_INET, &s->sin_addr, client_ip_buffer, buffer_size);
        }
        if (client_port)
        {
            *client_port = ntohs(s->sin_port);
        }
    }
    else // AF_INET6 IPv6
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
        if (client_ip_buffer && buffer_size > 0)
        {
            inet_ntop(AF_INET6, &s->sin6_addr, client_ip_buffer, buffer_size);
        }
        if (client_port)
        {
            *client_port = ntohs(s->sin6_port);
        }
    }

    return client_socket;
}

int net_receive(socket_t connected_socket, void *buffer, size_t buffer_size)
{
#ifdef _WIN32
    return recv(connected_socket, (char *)buffer, (int)buffer_size, 0);
#else
    return (int)recv(connected_socket, buffer, buffer_size, 0);
#endif
}

int net_send(socket_t connected_socket, const void *data, size_t length)
{
#ifdef _WIN32
    return send(connected_socket, (const char *)data, (int)length, 0);
#else
    return (int)send(connected_socket, data, length, 0);
#endif
}

socket_t net_connect(const char *host, uint16_t port)
{
    struct addrinfo hints, *res, *p;
    socket_t sock = INVALID_SOCKET_T;
    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%u", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
    {
        return INVALID_SOCKET_T;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET_T)
        {
            continue;
        }

        if (connect(sock, p->ai_addr, (int)p->ai_addrlen) < 0)
        {
            net_close_socket(sock);
            sock = INVALID_SOCKET_T;
            continue;
        }

        break; // Successfully connected
    }

    freeaddrinfo(res);

    return sock;
}

int net_get_socket_info(socket_t sock, char *ip_buffer, size_t buffer_size, uint16_t *port)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getsockname(sock, (struct sockaddr *)&addr, &addr_len) < 0)
    {
        return -1;
    }

    if (addr.ss_family == AF_INET)
    {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        if (ip_buffer && buffer_size > 0)
        {
            inet_ntop(AF_INET, &s->sin_addr, ip_buffer, buffer_size);
        }
        if (port)
        {
            *port = ntohs(s->sin_port);
        }
    }
    else // AF_INET6
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        if (ip_buffer && buffer_size > 0)
        {
            inet_ntop(AF_INET6, &s->sin6_addr, ip_buffer, buffer_size);
        }
        if (port)
        {
            *port = ntohs(s->sin6_port);
        }
    }

    return 0;
}

int net_send_all(socket_t connected_socket, const void *data, size_t length)
{
    const char *ptr = (const char *)data;
    size_t total_sent = 0;

    while (total_sent < length)
    {
        int bytes_sent = net_send(connected_socket, ptr + total_sent, length - total_sent);
        if (bytes_sent <= 0)
        {
            // Error or connection closed
            return -1;
        }
        total_sent += bytes_sent;
    }

    return 0; // Success
}

int net_receive_all(socket_t connected_socket, void *buffer, size_t length)
{
    char *ptr = (char *)buffer;
    size_t total_received = 0;

    while (total_received < length)
    {
        int bytes_received = net_receive(connected_socket, ptr + total_received, length - total_received);
        if (bytes_received <= 0)
        {
            // Error or connection closed before receiving all data
            return -1;
        }
        total_received += bytes_received;
    }

    return 0; // Success
}

void net_close_socket(socket_t sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}