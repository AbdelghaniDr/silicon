#include <set>
#include <silicon/api.hh>
#include <silicon/remote_api.hh>
#include <silicon/backends/websocketpp.hh>
#include <silicon/clients/javascript_client.hh>
#include "symbols.hh"

using namespace s;
using namespace sl;

// The html page
std::string index_html_source = R"html(
<html>
  <head>
    
    <title>Silicon Broadcast</title>
    <style>
      body { margin: 0; padding: 0; }
      #content { overflow:auto; height: 80%; overflow: auto; width: 100%; padding: 10px; }
      #form { padding: 10px; width: 100%; }
      #message_input { width: 95%; height: 30px; }
    </style>
    <script src="/js_client" type="text/javascript"></script>
  </head>
<body>
<div id="content"></div>
<form id="form"><input autocomplete=off autofocus id="message_input"/></form>
</body>

<script>
  var messages_div = document.getElementById('content');
  var input = document.getElementById('message_input');
  var ws = new silicon_json_websocket('ws://' + window.location.host);

  ws.api.message = function (m) { 
    var n = document.createElement('div');
    n.appendChild(document.createTextNode(m.text));
    messages_div.appendChild(n);
    messages_div.scrollTop = messages_div.scrollHeight;
  }

  document.getElementById('form').onsubmit = function (e) {
    if (input.value.length > 0) ws.broadcast({message: input.value });
    input.value = "";
    return false;
  };

</script>
)html";



int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cout << "Usage: " << argv[0] << " port " << std::endl;
    return 1;
  }
  
  using namespace sl;

  // The list of user
  std::set<wspp_connection> users;
  // A mutex to ensure thread safety of calls to users.erase and user.insert
  std::mutex users_mutex;
  
  // The remote client api accessible from this server.
  auto rclient = make_wspp_remote_client( _message * parameters(_text) );

  // The server websocket api accessible by the client.
  auto server_api = ws_api(

    // Broadcast a message to all clients.
    _broadcast * parameters(_message) = [&] (auto p) {
      for (const wspp_connection& c : users) rclient(c).message(p.message);
    }

    );
  
  // Generate javascript client.
  std::string js_client = generate_javascript_websocket_client(server_api);

  typedef std::lock_guard<std::mutex> lock;

  // Start the websocketpp server.
  wspp_json_serve(server_api, atoi(argv[1]),
                  _on_open = [&] (wspp_connection& c) { lock l(users_mutex); users.insert(c); },
                  _on_close = [&] (wspp_connection& c) { lock l(users_mutex); users.erase(c); },
                  _http_api = http_api(
                    GET / _js_client = [&] () { return js_client; },
                    GET / _home = [&] () { return index_html_source; }
                    )
    );

}
