#include "microtcp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include "../utils/crc32.h"

microtcp_header_t header;
void *temp_buffer_recv;
uint32_t *temp_size;
struct sockaddr_in *client_address, *server_address;
int client_sd, server_sd,flow_ctrl_win;
socklen_t server_address_len, client_address_len;
int r = 0;
int add_checksum(uint8_t *packet, int size)
{
  microtcp_header_t p;
  memcpy(&p, packet, 32);
  p.checksum = 0;
  memcpy(packet, &p, 32);
  uint32_t checksum = crc32(packet, size);
  memcpy(packet + 28, &checksum, 4);
  return 0;
}
int correct_checksum_packet(uint8_t *packet, int size)
{
  microtcp_header_t received_header;
  memcpy(&received_header, packet, sizeof(microtcp_header_t));

  uint32_t checksum = received_header.checksum;
  received_header.checksum = 0;
  memcpy(packet, &received_header, sizeof(microtcp_header_t));

  uint32_t calculated_checksum = crc32(packet, size);

  if (checksum != calculated_checksum)
  {
    return 0;
  }
  return 1;
}

void create_header(microtcp_sock_t *socket, uint16_t control_bits)
{
  uint8_t *buffer;
  buffer = malloc(sizeof(microtcp_header_t));
  header.seq_number = socket->seq_number;
  header.ack_number = socket->ack_number;
  header.control = control_bits;
  header.data_len = 0;
  header.checksum = 0;
  memcpy(buffer, &header, 32);
  header.checksum = crc32(buffer, sizeof(microtcp_header_t));
  free(buffer);
}
int correct_checksum(microtcp_header_t received_header)
{
  uint8_t *buffer;
  buffer = malloc(32);
  uint32_t checksum = received_header.checksum;
  received_header.checksum = 0;
  memcpy(buffer, &received_header, sizeof(microtcp_header_t));
  if (checksum != crc32(buffer, 32))
  {
    free(buffer);
    return 0;
  }
  free(buffer);
  return 1;
}
void print_header(microtcp_header_t *packet)
{
  printf(
      "Sequence number:%d\nAcknowledgement "
      "number:%d\nControl:%d\nChecksum:%d\n-------------------------\n",
      packet->seq_number, packet->ack_number, packet->control,
      packet->checksum);
}
microtcp_sock_t microtcp_socket(int domain, int type, int protocol)
{
  int sock;
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("Opening UDP listening socket");
    exit(EXIT_FAILURE);
  }
  microtcp_sock_t *socket = malloc(sizeof(microtcp_sock_t));
  socket = memset(socket, 0, sizeof(microtcp_sock_t));
  socket->sd = sock;
  socket->state = INVALID;
  socket->init_win_size = MICROTCP_WIN_SIZE;
  socket->curr_win_size = MICROTCP_WIN_SIZE;
  socket->cwnd = MICROTCP_INIT_CWND;
  socket->ssthresh = MICROTCP_INIT_SSTHRESH;
  return *socket;
}

int microtcp_bind(microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  if (bind(socket->sd, address, address_len) == -1)
  {
    perror("Couldnt bind socket");
    exit(1);
  }

  socket->state = LISTEN;
  return 0;
}

int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address,
                     socklen_t address_len)
{
  server_address = (struct sockaddr_in *)address;
  server_address_len = address_len;
  client_sd = socket->sd;
  socket->state = INVALID;
//  printf("Try connection to the server.....\n");
 // printf("\n3-Way handshake\n\n");
  send_syn(socket, (struct sockaddr *)address, address_len);
  receive_syn_ack_send_ack(socket, (struct sockaddr *)address, address_len);
 // printf("Connected!\n");
  socket->state = ESTABLISHED;
  socket->recvbuf = malloc(MICROTCP_RECVBUF_LEN);
  return 0;
}

int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address,
                    socklen_t address_len)
{
  socket->state = INVALID;
 // printf("Waiting to Accept......\n");
  receive_syn_send_SynAck(socket, address, address_len);
  receive_ack(socket, address, address_len);
  //printf("Accepted\n");
  server_sd = socket->sd;
  client_address = (struct sockaddr_in *)address;
  client_address_len = address_len;
  socket->ack_number++;
  socket->state = ESTABLISHED;
  socket->recvbuf = malloc(MICROTCP_RECVBUF_LEN);
  return 0;
}
int microtcp_shutdown(microtcp_sock_t *socket, int how)
{
  uint8_t *buffer = malloc(32);
  socket->state = CLOSING_BY_PEER;
  if (buffer == NULL)
  {
    // Handle memory allocation failure
    exit(EXIT_FAILURE);
  }

  // Optionally initialize the buffer
  memset(buffer, 0, 32);
  int length = 32;
  uint16_t control = 0;
  control |= (1 << 11);
  control |= (1 << 14);
  create_header(socket, control);
 // printf("FIN,ACK,seq=X:\n");
 // print_header(&header);
  ssize_t bytes_sent =
      sendto(socket->sd, &header, sizeof(microtcp_header_t), 0,
             (struct sockaddr *)server_address, server_address_len);
  socket->seq_number++;
  ssize_t bytes_received_ack = microtcp_recv(socket, buffer, length, 0);
  ssize_t bytes_received_fin_ack = microtcp_recv(socket, buffer, length, 0);
  socket->state = CLOSING_BY_HOST;
  //printf("ACK,seq=X+1,ack=Y+1:\n");
  send_ack(socket, (struct sockaddr *)server_address, server_address_len);
  socket->state = CLOSED;
  free(buffer);
  free(socket->recvbuf);
  return 0;
}
void send_ack(microtcp_sock_t *socket, struct sockaddr *address,
              socklen_t address_len)
{
  uint16_t control = 0;
  control |= (1 << 11);
  create_header(socket, control);
  //print_header(&header);
  ssize_t bytes_sent = sendto(socket->sd, &header, sizeof(microtcp_header_t), 0,
                              address, address_len);
}
int server_shutdown(microtcp_sock_t *socket)
{
  uint8_t *buffer = malloc(32);
  socket->state = CLOSING_BY_HOST;
  if (buffer == NULL)
  {
    // Handle memory allocation failure
    exit(EXIT_FAILURE);
  }

  // Optionally initialize the buffersend_CPack
  memset(buffer, 0, 32);
  int length = 32;
  uint16_t control = 0;
  control |= (1 << 11);
  create_header(socket, control);
 // printf("ACK,ack=X+1:\n");
 // print_header(&header);
  ssize_t bytes_sent_ack =
      sendto(socket->sd, &header, sizeof(microtcp_header_t), 0,
             (struct sockaddr *)client_address, client_address_len);
  socket->seq_number++; // NEW SEQ NUMBER Y
  control |= (1 << 14);
  create_header(socket, control);
 // printf("FIN,ACK,seq=Y:\n");
 // print_header(&header);
  ssize_t bytes_sent_fin_ack =
      sendto(socket->sd, &header, sizeof(microtcp_header_t), 0,
             (struct sockaddr *)client_address, client_address_len);
  socket->seq_number++;
  ssize_t bytes_received_ack = microtcp_recv(socket, buffer, length, 0);
  socket->state = CLOSED;
  free(buffer);
  free(socket->recvbuf);
  return bytes_received_ack;
}
void set_timeout(int receive_socket)
{
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec =
      MICROTCP_ACK_TIMEOUT_US;
  if (
      setsockopt(receive_socket, SOL_SOCKET,
                 SO_RCVTIMEO, &timeout,
                 sizeof(struct timeval)) < 0)
  {
    perror(" setsockopt");
  }
}
ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer,
                      size_t length, int flags)
{
  int bytes_sent = 0;
  uint32_t length_order = htonl(length);
  int remaining = length;
  int data_sent = 0;
  int b_sent;
  int start, end;
  int size;
  int new_start = -1;
  int current_seq = socket->seq_number;
  int duplicate_ack = 0;
  uint32_t ack_retransmit = 0;
  struct sockaddr *addr_to_send;
  socklen_t addr_len;
  if (socket->sd == client_sd)
  {
    addr_to_send = (struct sockaddr *)server_address;
    addr_len = server_address_len;
    b_sent = sendto(socket->sd, &length_order, sizeof(length_order), 0, (struct sockaddr *)server_address, server_address_len);
  }
  else
  {
    addr_to_send = (struct sockaddr *)server_address;
    addr_len = server_address_len;
    b_sent = sendto(socket->sd, &length_order, sizeof(length_order), 0, (struct sockaddr *)client_address, client_address_len);
  }
  //printf("\nsent:%d\n", b_sent);
  while (data_sent < length)
  {
    int min_temp = flow_ctrl_win < socket->cwnd ? flow_ctrl_win : socket->cwnd;
    int bytes_to_send = min_temp < remaining ? min_temp : remaining;
    if(bytes_to_send==0)
    {
      perror("bytes-send");
      exit(0);
    }
    int chunks = bytes_to_send / MICROTCP_MSS;
    int *check_size = malloc(chunks * sizeof(int));
      for (int i = 0; i < chunks; i++)
      {
        size = MICROTCP_MSS;
        start = i * MICROTCP_MSS;
        if (new_start != -1)
        {
          duplicate_ack = 0;
          start = new_start;
          socket->seq_number = ack_retransmit;
          bytes_sent = start;
    //      printf("\nbytes_send%d\n", bytes_sent);
        }
        if (i * MICROTCP_MSS < new_start)
        {
          continue;
        }
        else
        {
          new_start = -1;
        }
        create_header(socket, 0);

        socket->seq_number = socket->seq_number + size;
        size_t final_size = sizeof(microtcp_header_t) + size;
        check_size[i] = size;
        uint8_t *temp_buffer = (uint8_t *)malloc(final_size * sizeof(uint8_t));
        if (temp_buffer == NULL)
        {
          perror("Memory allocation failed");
          exit(EXIT_FAILURE);
        }
        memcpy(temp_buffer, &header, sizeof(microtcp_header_t));
      //  printf("header_sent:\n");
       // print_header(&header);
        memcpy(temp_buffer + sizeof(microtcp_header_t), buffer + start, size);
        add_checksum(temp_buffer, final_size);
        int chech;
        memcpy(&chech, temp_buffer + 28, 4);
       // printf("\n %d\n", chech);
        bytes_sent += sendto(socket->sd, temp_buffer, final_size, 0, addr_to_send, addr_len);
        bytes_sent -= 32;
      //  flow_ctrl_win -= bytes_sent;
        free(temp_buffer);
      }
      if (bytes_to_send % MICROTCP_MSS)
      {
        start = chunks * MICROTCP_MSS;
        size = bytes_to_send % MICROTCP_MSS;
        check_size[chunks] = size;
        chunks++;
        uint8_t *temp_buffer = (uint8_t *)malloc(size + sizeof(microtcp_header_t));
        if (temp_buffer == NULL)
        {
        }
        else
        {
          create_header(socket, 0);
          socket->seq_number = socket->seq_number + size;
          memcpy(temp_buffer, &header, sizeof(microtcp_header_t));
        //  printf("header_sent:\n");
         // print_header(&header);
          memcpy(temp_buffer + sizeof(microtcp_header_t), buffer + start, size);
          add_checksum(temp_buffer, size + sizeof(microtcp_header_t));
          bytes_sent += sendto(socket->sd, temp_buffer, bytes_to_send % MICROTCP_MSS + sizeof(microtcp_header_t), 0, addr_to_send, addr_len);
          bytes_sent -= 32;
        //  flow_ctrl_win-= bytes_sent;
          free(temp_buffer);
        }
      }
    for (int i = 0; i < chunks; i++)
    {
      microtcp_header_t header_received;
      int bytes_received;
      set_timeout(socket->sd);
      bytes_received = recvfrom(socket->sd, &header_received, sizeof(microtcp_header_t), 0, (struct sockaddr *)server_address, &server_address_len);
      if (bytes_received == -1)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          socket->ssthresh = socket->cwnd / 2;
          socket->cwnd = MICROTCP_MSS < socket->ssthresh ? MICROTCP_MSS : socket->ssthresh;
          duplicate_ack = 0;
          i = -1;
          data_sent = 0;
          remaining = length;
          bytes_sent = 0;
          continue;
        }
      }
      else
      {
       // flow_ctrl_win = header_received.window;
        if (socket->cwnd >= socket->ssthresh && i == 0) // congestion avoidance
        {
          socket->cwnd = socket->cwnd + MICROTCP_MSS;
        }
        if (socket->cwnd < socket->ssthresh) // slow start
        {
          socket->cwnd = socket->cwnd + MICROTCP_MSS;
        }
        if (current_seq + check_size[i] == header_received.ack_number) // check correct seq number in order
        {
          current_seq = header_received.ack_number;
        }
        else // out of order
        {
          ack_retransmit = header_received.ack_number;
          duplicate_ack++;
        }
        if (duplicate_ack > 0 && i == chunks - 1)
        {
          socket->ssthresh = socket->cwnd / 2;
          socket->cwnd = socket->cwnd / 2 + 1;
          new_start = start - (duplicate_ack - 1) * (MICROTCP_MSS);
        }
      }
     // printf("duplicate:%d\n", duplicate_ack);
     // printf("header_received:\n");
     // print_header(&header_received);
    }
    if (duplicate_ack != 0)
    {
      remaining -= bytes_to_send - (duplicate_ack - 1) * MICROTCP_MSS - bytes_sent % MICROTCP_MSS;
      data_sent += bytes_to_send - (duplicate_ack - 1) * MICROTCP_MSS - bytes_sent % MICROTCP_MSS;
    }
    else
    {

      remaining -= bytes_to_send;
      data_sent += bytes_to_send;
    }
    // free(duplicate_ack_buffer);
  }
  return bytes_sent;
}
ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length,
                      int flags)
{

  int bytes_recv = 0, total = 1;
  struct sockaddr_in *tmp = malloc(sizeof(struct sockaddr_in));
  microtcp_header_t tmp_header;
  int bytes_read;
  socklen_t temp_len = sizeof(tmp); // Initialize the length
  memset(&tmp, 0, sizeof(tmp));     // Initialize the sockaddr_in structure

  bytes_read = recvfrom(socket->sd, buffer, length, flags, (struct sockaddr *)tmp, &temp_len);

  if (bytes_read == -1)
  {
  //  printf("recvfrom failed with errno %d: %s\n", errno, strerror(errno));
    return -1; // Instead of exiting, return -1 to indicate an error
  }

  if (bytes_read == 32)
  {
    memcpy(&tmp_header, buffer, 32);
   // printf("Packet Received:\n");
    //print_header(&tmp_header);
    if (correct_checksum(tmp_header) == 0)
    {
      perror("Altered bits2");
    }
    if (tmp_header.control & (1 << 14) && tmp_header.control & (1 << 11))
    {
      if (socket->sd ==
          server_sd) // server
      {
        socket->ack_number = tmp_header.seq_number + 1; // ALLAGH
        socket->state = CLOSING_BY_PEER;
        int bytes = server_shutdown(socket);
        return bytes;
      }
      else if (socket->sd == client_sd) // client receive_fin_ack
      {
        socket->ack_number = tmp_header.seq_number + 1;
      }
      else
      {
        perror("wrong address 280");
      }
    }
    else if (tmp_header.control & (1 << 11))
    {
      if (socket->sd == server_sd) // server receive ack
      {
        if ((tmp_header.ack_number != socket->seq_number) ||
            (tmp_header.seq_number != socket->ack_number))
        {
          perror("not end:294");
        }
        else
        {
          return -1;
        }
      }
      else if (socket->sd == client_sd) // client receive ack
      {
        if (tmp_header.ack_number != socket->seq_number)
        {
          perror("Wrong ack number (client receive_ack)");
        }
      }
      else
      {
        perror("wrong address:304");
      }
    }
  }
  else
  {
    // Inside microtcp_recv
    if (bytes_read == 4)
    {
      temp_size = malloc(4);
      total--;
      if (temp_size == NULL)
      {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
      }
      memcpy(temp_size, buffer, 4);
      *(uint32_t *)temp_size = ntohl(*(uint32_t *)temp_size);
      //printf("\nData size to receive: %d\n", *(int *)temp_size); // Debugging
    }
    int number_loops = *(int *)temp_size / MICROTCP_MSS;
    number_loops += (*(int *)temp_size % MICROTCP_MSS) != 0 ? 1 : 0;
    int counter_duplicate = 0;
    for (int i = 0; i < number_loops; i++)
    {
      temp_buffer_recv = malloc(MICROTCP_MSS + sizeof(microtcp_header_t));
      if (temp_buffer_recv == NULL)
      {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
      }
      struct sockaddr_in received_tmp;
      socklen_t received_tmp_len = sizeof(received_tmp);
      memset(&received_tmp, 0, sizeof(received_tmp));
      set_timeout(socket->sd);
      bytes_recv = recvfrom(socket->sd, temp_buffer_recv, MICROTCP_MSS + sizeof(microtcp_header_t), flags, (struct sockaddr *)&received_tmp, &received_tmp_len);
      socket->curr_win_size-=bytes_recv-32;  
      if (bytes_recv == -1)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          perror("timeout");
          i = -1;
          total = 0;
          counter_duplicate = 0;
          continue;
        }
        else
        {
          perror("recvfrom failed");
          free(temp_buffer_recv);
          break;
        }
      }
      microtcp_header_t header_received;
      uint8_t *smallBuffer = malloc(bytes_recv);
      memcpy(smallBuffer, temp_buffer_recv, bytes_recv);
      memcpy(&header_received, temp_buffer_recv, sizeof(microtcp_header_t));
     // printf("\n");
      //printf("header received:\n");
      //print_header(&header_received);
      //printf("\n");
      uint8_t *payload = malloc(MICROTCP_MSS);
      int flag_checksum = correct_checksum_packet(smallBuffer, bytes_recv);
      //printf("checksum flag %d , bytes recv : %d\n", flag_checksum, bytes_recv);
      if (header_received.seq_number == socket->ack_number && flag_checksum == 1)
      {
        bytes_recv -= 32;
        total += bytes_recv;
        socket->ack_number = header_received.seq_number + bytes_recv;
        create_header(socket, 0);
        header.window=socket->curr_win_size+bytes_recv;
        socket->curr_win_size+=bytes_recv;
        memcpy(payload, temp_buffer_recv + sizeof(microtcp_header_t), bytes_recv);
        memcpy(buffer + total - bytes_recv, payload, bytes_recv);
        sendto(socket->sd, &header, sizeof(microtcp_header_t), 0, (struct sockaddr *)client_address, client_address_len);
      }
      else
      {
        create_header(socket, 0);
        sendto(socket->sd, &header, sizeof(microtcp_header_t), 0, (struct sockaddr *)client_address, client_address_len);
        counter_duplicate++;
        if (i == number_loops - 1)
        {
          i -= (counter_duplicate);
        }
      }
      //printf("\n");
      //printf("header sent:\n");
      //print_header(&header);
      //printf("\n");
      free(temp_buffer_recv);
      free(payload);
    }
  }
  free(tmp);
  free(temp_size);
  return total;
}
void send_syn(microtcp_sock_t *socket, struct sockaddr *address,
              socklen_t address_len)
{
  uint16_t control = 0;
  control |= (1 << 13);
  create_header(socket, control);
  header.window = socket->init_win_size;
  socket->seq_number++;
  //printf("SYN,seq=N\n");
  //print_header(&header);
  ssize_t bytes_sent = sendto(socket->sd, &header, sizeof(microtcp_header_t), 0,
                              address, address_len);
}
void receive_syn_send_SynAck(microtcp_sock_t *socket, struct sockaddr *address,
                             socklen_t address_len)
{
  uint8_t *buffer;
  uint32_t checksum;
  buffer = malloc(sizeof(microtcp_header_t));
  microtcp_header_t tmp;
  ssize_t bytes_received =
      recvfrom(socket->sd, buffer, 1024, 0, address, &address_len);
  flow_ctrl_win = *((uint16_t *)(buffer + 10));

  //printf("\n3-Way handshake\n\n");
  if (bytes_received < 0)
  {
    perror("Error receiving SYN packet");
    return;
  }
  //printf("Received packet:\n");
  memcpy(&tmp, buffer, sizeof(microtcp_header_t));
  //print_header(&tmp);
  if (correct_checksum(tmp) == 0)
  {
    //perror("Altered bits2");
  }
  if (tmp.control & (1 << 13))
  {
    uint16_t control = 0;
    control = tmp.control | (1 << 11);
    socket->ack_number = tmp.seq_number + 1;
    create_header(socket, control);
    header.window = socket->init_win_size;
    socket->seq_number++;
    //printf("SYN,ACK,seq=M,ack=N+1:\n");
    //print_header(&header);
    sendto(socket->sd, &header, sizeof(microtcp_header_t), 0, address,
           address_len);
  }
  else
  {
    perror("Received packet is not SYN");
    return;
  }
  free(buffer);
}
void receive_syn_ack_send_ack(microtcp_sock_t *socket, struct sockaddr *address,
                              socklen_t address_len)
{
  uint8_t *buffer;
  buffer = malloc(sizeof(microtcp_header_t));
  microtcp_header_t tmp;
  uint32_t checksum;

  ssize_t bytes_received = recvfrom(
      socket->sd, buffer, sizeof(microtcp_header_t), 0, address, &address_len);

  if (bytes_received < 0)
  {
    perror("Error receiving SYN packet");
    return;
  }
  flow_ctrl_win = *((uint16_t *)(buffer + 10));
  memcpy(&tmp, buffer, sizeof(microtcp_header_t));
  //printf("Packet Received:\n");
  //print_header(&tmp);
  if (correct_checksum(tmp) == 0)
  {
  //  perror("Altered bits3");
  }
  // Check if the received packet has both SYN and ACK flags set
  if ((tmp.control & (1 << 13)) && (tmp.control & (1 << 11)) &&
      (tmp.ack_number == socket->seq_number))
  {
    socket->ack_number = tmp.seq_number + 1;
    //printf("ACK,seq=N+1,ack=M+1\n");
    send_ack(socket, address, address_len);
    socket->seq_number++;
  }
  else
  {
    // Handle unexpected packet (not SYN-ACK)
    // You might want to log a message or return an error code
    perror("Received packet is not SYN-ACK");
    return;
  }
  free(buffer);
}
void receive_ack(microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  uint8_t *buffer;
  buffer = malloc(sizeof(microtcp_header_t));
  microtcp_header_t tmp;
  uint32_t checksum;
  ssize_t bytes_received = recvfrom(
      socket->sd, buffer, sizeof(microtcp_header_t), 0, address, &address_len);

  if (bytes_received < 0)
  {
    perror("Error receiving SYN packet");
    return;
  }
  memcpy(&tmp, buffer, sizeof(microtcp_header_t));
  if (correct_checksum(tmp) == 0)
  {
//    perror("Altered bits3");
  }
  if (!((tmp.control & (1 << 11)) && (tmp.ack_number == socket->seq_number) &&
        (tmp.seq_number == socket->ack_number)))
  {
    perror("Something went w1rong");
  }
  free(buffer);
}
