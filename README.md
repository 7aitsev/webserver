## About *WebServer* Project

It's just a tiny web server which handles web pages and other files as any
usual server does. It implements bare basics of HTTP 1.0: `GET` and `HEAD`
requests. That's it!

### What Do Those Requests Look Like?

Like this:

```text
GET / HTTP/1.0
```

You can read more about HTTP 1.0 requests on [www.w3.org][http_req]. A host
 header and all other headers like `User-Agent` are not supported,
i.e. simply ignored.

On a request the server sends bare-bones [HTTP 1.0 response][http_resp]
that looks like this:

```text
HTTP/1.0 200 OK
Content-type: text/html

<h1>Success!</h1>
```

The first line indicates the protocol (HTTP/1.0), the resulting status code
(`200`, in this case, means "you got it"), and the text of the status. The next
line sets the content type for the browser to know how to display the content.
Then a blank line, then the actual content.

### Tell Me More About This Beast!

No problem :)

This web server handles static content only in a synchronous multiplexing
manner. It does not support dynamic pages or even cgi-bin executables. Each
socket of a client is closed by the server as soon as all requested data has
been sent.

The server can be configured to work with a specified document root which
contains pages (and paths). It is possible to set a default location for
a home page (e.g. `index.html`)

It correctly serves content it finds and can read, and yields the appropriate
errors when it cannot:

* `400` for an invalid request
* `403` for permission denied (e.g. exists but it can't read it)
* `404` for a resource not found
* `500` for an internal server error
* `501` if a request contains not supported method
* `505` for a not supported HTTP version

[http_req]: https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request
[http_resp]: https://www.w3.org/Protocols/HTTP/1.0/spec.html#Response
