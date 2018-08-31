#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>


#include <sys/socket.h>
#include <arpa/inet.h>

#include "chatroom_utils.h"

void get_username(char *username,char *usr)
{
  while(true)
  {
    if(strlen(usr) > 20)
    {
      puts("Le nom d\'utilisateur doit comporter 20 caractères ou moins.");

    } else {
      break;
    }
  }
}

void set_username(connection_info *connection, char *username)
{
  message msg;
  msg.type = SET_USERNAME;
  strncpy(msg.username, username, 20);

  if(sendto(connection->socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&connection->address),sizeof(connection->address)) < 0)
  {
    perror("Echec de l\'envoi.");
    exit(1);
  }
}

void stop_client(connection_info *connection)
{
  close(connection->socket);
  exit(0);
}

void connect_to_server(connection_info *connection, char *address, char *port, char *username)
{

  while(true)
  {
    get_username(connection->username,username);

    if ((connection->socket = socket(AF_INET, SOCK_STREAM , IPPROTO_TCP)) < 0)
    {
        perror("socket ne crée pas.");
    }

    connection->address.sin_addr.s_addr = inet_addr(address);
    connection->address.sin_family = AF_INET;
    connection->address.sin_port = htons(atoi(port));
    
    set_username(connection,username);
    break;
  }


  puts("Connecté au serveur.");
  puts("Taper _help pour usage.");
}


void handle_user_input(connection_info *connection)
{
  char input[255];
  fgets(input, 255, stdin);
  trim_newline(input);

  if(strncmp(input, "_connect",4) == 0)
  {
    char *username = strtok(input+8, " ");
    char *server = strtok(NULL, " ");
    char *port = strtok(NULL, " ");

    connect_to_server(connection, server, port, username);
  }
  else if(strcmp(input, "_quit") == 0)
  {
    stop_client(connection);
  }
  else if(strcmp(input, "_who") == 0)
  {
    message msg;
    msg.type = GET_USERS;

    if(sendto(connection->socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&connection->address),sizeof(connection->address)) < 0)
    {
        perror("Echec de l\'envoi");
        return;
    }
  }
  else if(strcmp(input, "_help") == 0)
  {
    puts("_quit: quitter le programe.");
    puts("_help : Affiche des informations d\'aide.");
    puts("_who: Affiche la liste des utilisateurs connectés au serveur.");
    puts("_m <username> <message> Envoyer un message privé à un nom d\'utilisateur donné.");
    puts("tout autre message sera diffusé pour tous les utilisateurs connectés.");
  }
  else if(strncmp(input, "_m", 2) == 0)
  {
    message msg;
    msg.type = PRIVATE_MESSAGE;

    char *toUsername, *chatMsg;

    toUsername = strtok(input+3, " ");

    if(toUsername == NULL)
    {
      puts(KRED "Le format des messages privés est: /m <username> <message>" RESET);
      return;
    }

    if(strlen(toUsername) == 0)
    {
      puts(KRED "Vous devez entrer un nom d\'utilisateur pour un message privé." RESET);
      return;
    }

    if(strlen(toUsername) > 20)
    {
      puts(KRED "Le nom d\'utilisateur doit comporter entre 1 et 20 caractères." RESET);
      return;
    }

    chatMsg = strtok(NULL, "");

    if(chatMsg == NULL)
    {
      puts(KRED "Vous devez entrer un message à envoyer à l\'utilisateur spécifié." RESET);
      return;
    }

    strncpy(msg.username, toUsername, 20);
    strncpy(msg.data, chatMsg, 255);

    if(sendto(connection->socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&connection->address),sizeof(connection->address)) < 0)
    {
        perror("Echec de l\'envoi");
        return;
    }

  }
  else
  {
    message msg;
    msg.type = PUBLIC_MESSAGE;
    strncpy(msg.username, connection->username, 20);

    if(strlen(input) == 0) {
        return;
    }

    strncpy(msg.data, input, 255);

    if(sendto(connection->socket, (void*)&msg, sizeof(msg), 0,((struct sockaddr *)&connection->address),sizeof(connection->address)) < 0)
    {
        perror("Echec de l\'envoi");
        return;
    }
  }



}

void handle_server_message(connection_info *connection)
{
  message msg;

  //Receive a reply from the server
  ssize_t recv_val =recvfrom(connection->socket, &msg, sizeof(message), 
                0, ( struct sockaddr *) &connection->address,
                ((socklen_t *)sizeof(connection->address)));
  if(recv_val < 0)
  {
      perror("recv failed");
      return;

  }
  else if(recv_val == 0)
  {
    close(connection->socket);
    puts("Server disconnected.");
    exit(0);
  }

  switch(msg.type)
  {

    case CONNECT:
      printf(KYEL "%s a connecté." RESET "\n", msg.username);
    break;

    case DISCONNECT:
      printf(KYEL "%s s\'est déconnecté." RESET "\n" , msg.username);
    break;

    case GET_USERS:
      printf("%s", msg.data);
    break;

    case SET_USERNAME:
      
    break;

    case PUBLIC_MESSAGE:
      printf(KGRN "%s" RESET ": %s\n", msg.username, msg.data);
    break;

    case PRIVATE_MESSAGE:
      printf(KWHT "De %s:" KCYN " %s\n" RESET, msg.username, msg.data);
    break;

    case TOO_FULL:
      fprintf(stderr, KRED "Le serveur est trop plein pour accepter de nouveaux clients." RESET "\n");
      exit(0);
    break;

    default:
      fprintf(stderr, KRED "Type de message inconnu reçu." RESET "\n");
    break;
  }
}

int main(int argc, char *argv[])
{
  connection_info connection;
  fd_set file_descriptors;

  while(true)
  {
    FD_ZERO(&file_descriptors);
    FD_SET(STDIN_FILENO, &file_descriptors);
    FD_SET(connection.socket, &file_descriptors);
    fflush(stdin);

    if(select(connection.socket+1, &file_descriptors, NULL, NULL, NULL) < 0)
    {
      perror("la sélection de la connexion a échoué.");
      return 0;
    }

    if(FD_ISSET(STDIN_FILENO, &file_descriptors))
    {
      handle_user_input(&connection);
    }

    if(FD_ISSET(connection.socket, &file_descriptors))
    {
      handle_server_message(&connection);
    }
  }

  close(connection.socket);
  return 0;
}


