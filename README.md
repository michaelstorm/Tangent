# Tangent

Tangent is a modern implementation of the [Chord](http://en.wikipedia.org/wiki/Chord_%28peer-to-peer%29) peer-to-peer
routing protocol in C, with an eye toward real-life deployments on the open Internet. It's based on
[Berkeley's implementation](http://i3.cs.berkeley.edu/download/i3_0_3.tar.gz) from their [i3](http://i3.cs.berkeley.edu/)
project. I'm grateful to the original implementors for their excellent work.

However, the original version seemed to be unmaintained. So, I drastically refactored it in hopes of using it in another
project, adding some (hopefully) interesting new features in the process.

## New features

* ### Libevent core
  Tangent is single-threaded and event-oriented with [libevent](http://libevent.org/), in the hope that I/O
  will dominate performance. Of course, I have not yet benchmarked a single thing, but by eyeballing it, that should
  be the case. The original version used simply a select() call, which wasn't extensible. Libevent, on the other hand,
  both cooperates with external event systems and invokes more modern system calls under the hood, such as epoll()
  on Linux 2.6+.

* ### DDoS mitigation
  For every request/response packet pair, Tangent creates a "ticket" from a hash of a private secret + all parameters
  that are shared between the request and response packets, then embeds that ticket in the request. Tangent verifies that
  the ticket included in every response matches some ticket that it had generated by performing the same operations
  on the response parameters. This prevents a peer from sending malicious responses to non-existent queries, but it does
  not prevent many kinds of attacks - for example, replay attacks. There are trade-offs associated with mitigating further
  attack categories, but they'll be a subject of future investigation.
  
  Also, nodes are restricted to owning the section of the namespace circle ending with a hash of their address and port
  (see "Automatic address discovery" below). This isn't perfect either, but it does prevent nodes from claiming arbitrary
  parts of the namespace. This protection may weaken as IPv6 becomes more widespread, since nodes may have greater
  freedom in choosing their addresses.

* ### Seamless IPv6 support
  Tangent stores all peer addresses internally as IPv6 addresses by transforming IPv4 addresses to
  [IPv4-mapped IPv6 addresses](http://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses). This allows a natural
  bridging between IPv4 and IPv6 peers, provided that the peers understand that routing may not be transitive. That is,
  some peers may be IPv4-only, some IPv6-only, and some both IPv4- and IPv6-capable. This will be another subject of
  investigation.

* ### Protocol Buffers-based messaging format
  To encourage forward-compatibility, messages are defined in the
  [Protocol Buffers](https://developers.google.com/protocol-buffers/docs/overview) format. This also makes inspection
  of packet contents easier.

* ### Automatic address discovery
  Each Tangent peer uses a hash of its address and port to determine its position along the namespace circle. But to
  determine its own address in the first place (due to [NAT](https://en.wikipedia.org/wiki/Network_address_translation)),
  the peer asks the opinion of its own address from a list of "bootstrap" nodes. Currently, it uses the first response,
  but it should use the plurality response.

* ### Improved logging
  I wrote yet-another logging library for this project, this time in pure C. I'll probably rip it out and into a separate
  library at some point. It's something of an attempt to write a logger outside of the Log4j idiom, and instead using C
  idioms, such as first-class support for passing around FILE pointers (which allows reuse of debug printing functions).

* ### Embeddable shared library implementation
  I don't recall the original version being a shared library - instead, it forked and communicated with its subprocess
  via a UNIX socket. My memory is a little hazy on this point. However, this project makes a point to allow safe embedding
  of Chord within larger programs. Hence, for example, the -fvisibility=hidden flag to GCC, to ensure that only selected
  symbols are exported. (The external API needs to be formalized, at some point. It's a half-constructed mess right now.)