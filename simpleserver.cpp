/* run using ./server <port> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include <pthread.h>
#include "http_server.hh"

#include <vector>

#include <sys/stat.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <queue>

#define noofthreads 10
#define maxsize 100000000

using namespace std;

queue<int> fd;

pthread_mutex_t mutexfd;
pthread_cond_t condfd;

HTTP_Response *response1;
vector<string> split(const string &s, char delim)
{
  vector<string> elems;

  stringstream ss(s);
  string item;

  while (getline(ss, item, delim))
  {
    if (!item.empty())
      elems.push_back(item);
  }

  return elems;
}

HTTP_Request::HTTP_Request(string request)
{
  vector<string> lines = split(request, '\n');
  vector<string> first_line = split(lines[0], ' ');

  this->HTTP_version = "1.0"; // We'll be using 1.0 irrespective of the request

  /*
   TODO : extract the request method and URL from first_line here
  */

  this->method = first_line[0];
  this->url = first_line[1];

  if (this->method != "GET")
  {
    cerr << "Method '" << this->method << "' not supported" << endl;
    // exit(1);
  }
}

HTTP_Response *handle_request(string req)
{
  // res="";
  HTTP_Request *request = new HTTP_Request(req);

  HTTP_Response *response = new HTTP_Response();

  string url = string("html_files") + request->url;

  response->HTTP_version = "1.0";

  struct stat sb;
  if (stat(url.c_str(), &sb) == 0) // requested path exists
  {
    response->status_code = "200";
    response->status_text = "OK";
    response->content_type = "text/html";

    string body;

    if (S_ISDIR(sb.st_mode))
    {
      /*
      In this case, requested path is a directory.
      TODO : find the index.html file in that directory (modify the url
      accordingly)
      */
      url = url + "/index.html";
    }

    /*
    TODO : open the file and read its contents
    */
    ifstream ifs(url);
    string c((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

    response->body = c;
    response->content_length = to_string(c.size());
  }

  else
  {
    response->status_code = "404";
    response->status_text = "Not Found";
    response->content_type = "text/html";
    string c = "404 Not Found";
    response->content_length = to_string(c.size());
    response->body = c;
    /*
    TODO : set the remaining fields of response appropriately
    */
  }
  response1 = response;
  delete request;
  
  return response;
}

string HTTP_Response::get_string(int newsockfd)
{
  /*
  TODO : implement this function
  */

  /* WRITE THE CODE FOR ELSE PART 404: */
  int stcode = std::stoi(response1->status_code);
  string header = "HTTP/1.0 " + response1->status_code + " " + response1->status_text + "\n" + "Content-Type: " + response1->content_type + "\n" + "Content-Length: " + response1->content_length;
  header = header + "\n\n" + response1->body;
  int n;
  char arr[header.length() + 1];
  strcpy(arr, header.c_str());
  if (stcode == 200)
  {
    n = write(newsockfd, arr, sizeof(arr));
  }
  else if (stcode == 404)
  {
    n = write(newsockfd, "404 Not Found", 13);
  }
  close(newsockfd);
  //sleep(20);
  // delete response1;
  return "";
}

void error(char *msg)
{
  perror(msg);
  // exit(1);
}

void sockettoqueue(int newsocketfd)
{
  pthread_mutex_lock(&mutexfd);
  if(fd.size()<=10000)
  {
    fd.push(newsocketfd);
  }
  pthread_mutex_unlock(&mutexfd);
  pthread_cond_signal(&condfd);
}

void *threadfun(void *y)
{
  int n;
  char buffer[4096];
  int x;

  while (1)
  {

    bzero(buffer, 4096);
    pthread_mutex_lock(&mutexfd);
    while (fd.size() == 0)
    {
      pthread_cond_wait(&condfd, &mutexfd);
    }

    if (fd.size() != 0)
    {
      x = fd.front();
      fd.pop();
    }
    pthread_mutex_unlock(&mutexfd);

    n = read(x, buffer, 4096);
    if (n <= 0)
      break;

    string buf(buffer);

    //cout << buf << endl;

    HTTP_Response *httpresp = handle_request(buf);
    httpresp->get_string(x);
    delete httpresp;
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  int sockfd, portno, newsockfd, n;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  pthread_t th_id[noofthreads];

  pthread_mutex_init(&mutexfd, NULL);
  pthread_cond_init(&condfd, NULL);


  if (argc < 2)
  {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  for (int i = 0; i < noofthreads; i++)
  {
    if (pthread_create(&th_id[i], NULL, &threadfun, NULL) != 0)
    {
      perror("Failed to create the thread");
    }
  }
  /* create socket */

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  /* fill in port number to listen on. IP address can be anything (INADDR_ANY)
   */

  bzero((char *)&serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* bind socket to this port number on this machine */

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  /* listen for incoming connection requests */
  listen(sockfd, 5);
  clilen = sizeof(cli_addr);

  /* accept a new request, create a newsockfd */

  while (1)
  {
    /* read message from client */
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
      error("ERROR on accept");
    sockettoqueue(newsockfd);
  }
  delete response1;
  return 0;
}
