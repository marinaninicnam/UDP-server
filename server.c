#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>

#include "chatroom_utils.h"

#define MAX_CLIENTS 4

void initialize_server(connection_info *server_info, int port)
{
  if((server_info->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("la création de socket a échoué.");
    return;
  }
  server_info->address.sin_family = AF_INET;
  server_info->address.sin_addr.s_addr = INADDR_ANY;
  server_info->address.sin_port = htons(port);

  if(bind(server_info->socket, (struct sockaddr *)&server_info->address, sizeof(server_info->address)) < 0)
  {
    perror("Bind a échoué.");
    return;
  }

  const int optVal = 1;
  const socklen_t optLen = sizeof(optVal);
  if(setsockopt(server_info->socket, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen) < 0)
  {
    perror("échec à définir l\'option pour socket.");
    return;
  }


  

  //Accept and incoming connection
  printf("Le serveur est prêt pour les connexions...\n");
}

void send_public_message(connection_info clients[], int sender, char *message_text)
{
  message msg;
  msg.type = PUBLIC_MESSAGE;
  strncpy(msg.username, clients[sender].username, 20);
  strncpy(msg.data, message_text, 256);
  int i = 0;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(i != sender && clients[i].socket != 0)
    {
      if(sendto(clients[i].socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&clients[i].address),sizeof(clients[i].address)) < 0)
      {
          perror("Echec de l\'envoi");
          return;
      }
    }
  }
}

void send_private_message(connection_info clients[], int sender,
  char *username, char *message_text)
{
  message msg;
  msg.type = PRIVATE_MESSAGE;
  strncpy(msg.username, clients[sender].username, 20);
  strncpy(msg.data, message_text, 256);

  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(i != sender && clients[i].socket != 0
      && strcmp(clients[i].username, username) == 0)
    {
      if(sendto(clients[i].socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&clients[i].address),sizeof(clients[i].address)) < 0)
      {
          perror("Echec de l\'envoi");
          return;
      }
      return;
    }
  }

  msg.type = USERNAME_ERROR;
  sprintf(msg.data, "Nom d\'utilisateur \"%s\" n\'existe pas ou n\'est pas connecté.", username);

  if(sendto(clients[sender].socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&clients[sender].address),sizeof(clients[sender].address)) < 0)
  {
       perror("Echec de l'envoi");
      return;
  }

}

void send_connect_message(connection_info *clients, int sender)
{
  message msg;
  msg.type = CONNECT;
  strncpy(msg.username, clients[sender].username, 21);
  int i = 0;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(clients[i].socket != 0)
    {
      if(i == sender)
      {
        msg.type = SUCCESS;
        if(sendto(clients[i].socket, &msg, sizeof(msg), 0,((struct sockaddr *)&clients[i].address),sizeof(clients[i].address)) < 0)
        {
            perror("Echec de l\'envoi.");
            return;
        }
      }else
      {
        if(sendto(clients[i].socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&clients[i].address),sizeof(clients[i].address)) < 0)
        {
            perror("Echec de l\'envoi.");
            return;
        }
      }
    }
  }
}

void send_disconnect_message(connection_info *clients, char *username)
{
  message msg;
  msg.type = DISCONNECT;
  strncpy(msg.username, username, 21);
  int i = 0;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(clients[i].socket != 0)
    {
      if(sendto(clients[i].socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&clients[i].address),sizeof(clients[i].address)) < 0)
      {
          perror("Echec de l\'envoi.");
          return;
      }
    }
  }
}

void send_user_list(connection_info *clients, int receiver) {
  message msg;
  msg.type = GET_USERS;
  char *list = msg.data;

  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(clients[i].socket != 0)
    {
      list = stpcpy(list, clients[i].username);
      list = stpcpy(list, "\n");
    }
  }

  if(sendto(clients[receiver].socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&clients[receiver].address),sizeof(clients[receiver].address)) < 0)
  {
      perror("Echec de l\'envoi.");
      return;
  }

}

void stop_server(connection_info connection[])
{
  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    close(connection[i].socket);
  }
  exit(0);
}


void handle_client_message(connection_info clients[], int sender)
{
  int read_size;
  message msg;

    if((read_size = recvfrom(clients[sender].socket, &msg, sizeof(message), 
                0, ( struct sockaddr *) &clients[sender].address,
                ((socklen_t *)sizeof(clients[sender].address)))) == 0)
  {
    printf("Utilisateur déconnecté: %s.\n", clients[sender].username);
    close(clients[sender].socket);
    clients[sender].socket = 0;
    send_disconnect_message(clients, clients[sender].username);

  } else {

    switch(msg.type)
    {
      case GET_USERS:
        send_user_list(clients, sender);
      break;

      case SET_USERNAME: ;
        int i;
        for(i = 0; i < MAX_CLIENTS; i++)
        {
          if(clients[i].socket != 0 && strcmp(clients[i].username, msg.username) == 0)
          {
            close(clients[sender].socket);
            clients[sender].socket = 0;
            return;
          }
        }

        strcpy(clients[sender].username, msg.username);
        printf("Utilisateur connecté: %s\n", clients[sender].username);
        send_connect_message(clients, sender);
      break;

      case PUBLIC_MESSAGE:
        send_public_message(clients, sender, msg.data);
      break;

      case PRIVATE_MESSAGE:
        send_private_message(clients, sender, msg.username, msg.data);
      break;

      default:
        fprintf(stderr, "Type de message inconnu reçu.\n");
      break;
    }
  }
}

int construct_fd_set(fd_set *set, connection_info *server_info,
                      connection_info clients[])
{
  FD_ZERO(set);
  FD_SET(STDIN_FILENO, set);
  FD_SET(server_info->socket, set);

  int max_fd = server_info->socket;
  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(clients[i].socket > 0)
    {
      FD_SET(clients[i].socket, set);
      if(clients[i].socket > max_fd)
      {
        max_fd = clients[i].socket;
      }
    }
  }
  return max_fd;
}

void send_too_full_message(int socket,connection_info *server_info)
{
  message too_full_message;
  too_full_message.type = TOO_FULL;

  if(sendto(socket, (void*)&too_full_message, sizeof(too_full_message), 0,((struct sockaddr *)&server_info->address)
					,(sizeof(server_info->address)) 
	       ) < 0)
  {
      perror("Send too full message failed");
      return;
  }

  close(socket);
}

void handle_new_connection(connection_info *server_info, connection_info clients[])
{
  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(clients[i].socket == 0) {
      clients[i].socket = server_info->socket;
      break;

    } else if (i == MAX_CLIENTS -1) // if we can accept no more clients
    {
      send_too_full_message(server_info->socket, server_info);
    }
  }
}

void list_all_clients(connection_info clients[])
{
  int i;
puts("Utilisateur connecté:");
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    if(clients[i].socket != 0)
    {
	printf("%s\n", clients[i].username);
    }
  }
}

void kill_client_connection(connection_info clients[],char *input)
{
    int i;
    int clientFound = 0;
    char *username;

    username = strtok(input+6, " ");
    
    if(username == NULL)
    {
      puts(KRED "Le format pour _kill is: _kil <username>" RESET);
      return;
    }

    if(strlen(username) == 0)
    {
      puts(KRED "Vous devez entrer un nom d'utilisateur pour _kill." RESET);
      return;
    }

    for(i = 0; i < MAX_CLIENTS; i++)
    {
      if(clients[i].socket != 0 && strcmp(clients[i].username, username)==0)
      {
	clientFound = 1;
	printf("Utilisateur déconnecté: %s.\n", clients[i].username);
    	close(clients[i].socket);
    	clients[i].socket = 0;
    	send_disconnect_message(clients, clients[i].username);
      }
    }
    if(clientFound==0)
    {
	puts(KRED "_kill <username> : le nom d'utilisateur n'existe pas." RESET);
        return;
    }
}

void handle_user_input(connection_info clients[])
{
  char input[255];
  //fgets(input, sizeof(input), stdin);
  fgets(input, 255, stdin);
  trim_newline(input);

  if(strcmp(input, "_shutdown") == 0) {
    stop_server(clients);
  }

  if(strcmp(input, "_who") == 0) {
    list_all_clients(clients);
  }

  if(strncmp(input, "_kill",5) == 0) {
    kill_client_connection(clients,input);
  }
}

int main(int argc, char *argv[])
{
  puts("le serveur est en marche.");

  fd_set file_descriptors;

  connection_info server_info;
  connection_info clients[MAX_CLIENTS];

  int i;
  for(i = 0; i < MAX_CLIENTS; i++)
  {
    clients[i].socket = 0;
  }

  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return 0;
  }

  initialize_server(&server_info, atoi(argv[1]));

  while(true)
  {
    int max_fd = construct_fd_set(&file_descriptors, &server_info, clients);

    if(select(max_fd+1, &file_descriptors, NULL, NULL, NULL) < 0)
    {
      perror("la sélection de la connexion a échoué.");
      stop_server(clients);
    }

    if(FD_ISSET(STDIN_FILENO, &file_descriptors))
    {
      handle_user_input(clients);
    }

    if(FD_ISSET(server_info.socket, &file_descriptors))
    {
      handle_new_connection(&server_info, clients);
    }

    for(i = 0; i < MAX_CLIENTS; i++)
    {
      if(clients[i].socket > 0 && FD_ISSET(clients[i].socket, &file_descriptors))
      {
        handle_client_message(clients, i);
      }
    }
  }

  return 0;
}
