- [Lab 0: networking warmup](#lab-0-networking-warmup)
  - [3 Writing a network program using an OS stream socket](#3-writing-a-network-program-using-an-os-stream-socket)
    - [3.1 Get started](#31-get-started)
    - [3.2 Modern C++: mostly safe but still fast and low-level](#32-modern-c-mostly-safe-but-still-fast-and-low-level)
    - [3.3 Reading the Minnow support code](#33-reading-the-minnow-support-code)
    - [3.4 Writing webget](#34-writing-webget)
  - [4 An in-memory reliable byte stream](#4-an-in-memory-reliable-byte-stream)
- [Lab 1: stitching substrings into a byte stream](#lab-1-stitching-substrings-into-a-byte-stream)
  - [1 Getting started](#1-getting-started)
  - [2 Putting substrings in sequence](#2-putting-substrings-in-sequence)
    - [2.1 What should the Reassembler store internally?](#21-what-should-the-reassembler-store-internally)
    - [2.2 FAQs](#22-faqs)
- [Lab 2: the TCP receiver](#lab-2-the-tcp-receiver)
  - [1 Translating between 64-bit indexes and 32-bit seqnos](#1-translating-between-64-bit-indexes-and-32-bit-seqnos)
  - [2 Implementing the TCP receiver](#2-implementing-the-tcp-receiver)
    - [2.1 receive()](#21-receive)

<style>
  .center {
  width: auto;
  display: table;
  margin-left: auto;
  margin-right: auto;
}
</style>

# Lab 0: networking warmup
## 3 Writing a network program using an OS stream socket
### 3.1 Get started
Fetch the source code: `git clone https://github.com/cs144/minnow ` 

Enter the Lab 0 directory: `cd minnow`

Create a directory to compile the lab software: `cmake -S . -B build`

Compile the source code: `cmake --build build`
### 3.2 Modern C++: mostly safe but still fast and low-level
C++ standard:
1. Use the language documentation at https://en.cppreference.com as a resource. (We'd recommend you avoid cplusplus.com which is more likely to be out-of-date.)
2. Never use `malloc()` or `free()`.
3. Never use `new` or `delete`.
4. Essentially never use raw pointers (`*`), and use "smart" pointers (`unique_ptr` or `shared_ptr`) only when necessary. (You will not need to use these in CS144.)
5. Avoid templates, threads, locks, and virtual functions. (You will not need to use these in CS144.)
6. Avoid C-style strings (`char *str`) or string functions (`strlen(), strcpy()`). These are pretty error-prone. Use a `std::string` instead.
7. Never use C-style casts (e.g., `(FILE *)x`). Use a C++ `static_cast` if you have to (you generally will not need this in CS144).
8. Prefer passing function arguments by const reference (e.g.: `const Address & address`).
9. Make every variable const unless it needs to be mutated.
10. Make every method const unless it needs to mutate the object.
11. Avoid global variables, and give every variable the smallest scope possible.
12. Before handing in an assignment, run `cmake --build build --target tidy` for suggestions on how to improve the code related to C++ programming practices, and `cmake --build build --target format` to format the code consistently

### 3.3 Reading the Minnow support code
Interface: [socket.hh](./minnow/util/socket.hh) and [file_descriptor.hh](./minnow/util/file_descriptor.hh)

### 3.4 Writing webget
To implement: [webget.cc](./minnow/apps/webget.cc), a program to fetch Web pages over the Internet using the operating system's TCP support and stream-socket abstraction. Use the [TCPSocket](./minnow/util/socket.hh) and [Address](./minnow/util/address.hh) classes.

Hints:
1. Please note that in HTTP, each line must be ended with "\r\n"
2. Don't forget to include the "Connection: close" line in your client's request.
3. Make sure to read and print all the output from the server until the socket reaches "EOF" (end of file) --- a single call to `read` is not enough.

Test: `cmake --build build --target check webget`

## 4 An in-memory reliable byte stream
To implement: [stream.hh](./minnow/src/byte_stream.hh) and [stream.cc](./minnow/src/byte_stream.cc)
Methods to implement:
1. `vector<char>`: time exceeds
2. `deque<char>`: 0.31 Gbit/s
3. `string`: 1.09 Gbit/s
4. `deque<string_view>` and `deque<string>`: 9.79 Gbit/s

`string_view.remove_prefix()` is much faster than `string.substr()` and `deque.erase()`.

About `string_view`:
- [C++17剖析：string_view的实现，以及性能](https://zhuanlan.zhihu.com/p/166359481)
- [C++ 17 std::string_view使用介绍](https://www.cnblogs.com/yangxunwu1992/p/14018837.html)

Notice:
- `c_str()` has the same lifetime as the `string` object.

# Lab 1: stitching substrings into a byte stream
## 1 Getting started
1. Make sure you have committed all your solutions to Checkpoint 0. Please don't modify any files outside of the *src* directory, or *webget.cc*. You may have trouble merging the Checkpoint 1 starter code otherwise.
2. While inside the repository for the lab assignments, run git fetch to retrieve the most recent version of the lab assignments.
3. Download the starter code for Checkpoint 1 by running `git merge origin/check1-startercode`.
4. Make sure your build system is properly set up: `cmake -S . -B build`
5. Compile the source code: `cmake --build build`

## 2 Putting substrings in sequence
The TCP sender is dividing its byte stream up into short segments (substrings no more than about 1,460 bytes apiece) so that they each fit inside a datagram. But the network might reorder these datagrams, or drop them, or deliver them more than once. The receiver must reassemble the segments into the contiguous stream of bytes that they started out as.

In this lab you'll write the data structure that will be responsible for this reassembly: *a Reassembler*. It will receive substrings, consisting of a string of bytes, and the index of the first byte of that string within the larger stream. **Each byte of the stream** has its own unique index, starting from zero and counting upwards.

The full (public) interface of the reassembler is described by the *Reassembler* class in the [reassembler.hh](./minnow/src/reassembler.hh) header. Your task is to implement this class. You may add any private members and member functions you desire to the *Reassembler* class, but you cannot change its public interface.

### 2.1 What should the Reassembler store internally?
1. Bytes that are the **next bytes** in the stream. The Reassembler should push these to the Writer as soon as they are known.
2. Bytes that fit within the stream's available capacity but can't yet be written, because earlier bytes remain unknown. These should be stored internally in the Reassembler.
3. Bytes that lie beyond the stream's available capacity. These should be discarded. The Reassembler's will not store any bytes that can't be pushed to the ByteStream either immediately, or as soon as earlier bytes become known.

The goal of this behavior is to **limit the amount of memory** used by the Reassembler and ByteStream, no matter how the incoming substrings arrive. We've illustrated this in the picture below. The "capacity" is an upper bound on *both*:
1. The number of bytes buffered in the reassembled ByteStream (shown in green), and
2. The number of bytes that can be used by "unassembled" substrings (shown in red)

![Alt text](images/check1.png)

### 2.2 FAQs
- *What is the index of the first byte in the whole stream?*
Zero.
- *How efficient should my implementation be?*
    The choice of data structure is again important here. Please don't take this as a challenge to build a grossly space- or time-inefficient data structure -- the Reassembler will be the foundation of your TCP implementation. You have a lot of options to choose from. We have provided you with a benchmark; anything greater than 0.1 Gbit/s (100 megabits per second) is acceptable. A top-of-the-line Reassembler will achieve 10 Gbit/s.
- *How should inconsistent substrings be handled?* 
You may assume that they don't exist. That is, you can assume that there is a unique underlying byte-stream, and all substrings are (accurate) slices of it.
- *What may I use?* 
You may use any part of the standard library you find helpful. In particular, we expect you to use at least one data structure.
- *When should bytes be written to the stream?* 
As soon as possible. The only situation in which a byte should not be in the stream is that when there is a byte before it that has not been "pushed" yet.
- *May substrings provided to the `insert()` function overlap?* 
Yes.
- *Will I need to add private members to the Reassembler?* 
Yes. Substrings may arrive in any order, so your data structure will have to "remember" substrings until they're ready to be put into the stream -- that is, until all indices before them have been written.
- *Is it okay for our re-assembly data structure to store overlapping substrings?* 
No. It is possible to implement an "interface-correct" reassembler that stores overlapping substrings. But allowing the re-assembler to do this undermines the notion of "capacity" as a memory limit. If the caller provides redundant knowledge about the same index, the Reassembler should only store one copy of this information.

# Lab 2: the TCP receiver
TCP is a protocol that reliably conveys a pair of flow-controlled byte streams (one in each direction) over unreliable datagrams. Two parties, or "peers," participate in the TCP connection, and each peer acts as both "sender" (of its own outgoing byte stream) and "receiver" (of an incoming byte stream) at the same time.

This week, you'll implement the "receiver" part of TCP, responsible for receiving messages from the sender, reassembling the byte stream (including its ending, when that occurs), and determining that messages that should be sent back to the sender for acknowledgment and flow control.

## 1 Translating between 64-bit indexes and 32-bit seqnos
As a warmup, we'll need to implement TCP's way of representing indexes. Last week you created a *Reassembler* that reassembles substrings where each individual byte has a 64-bit **stream index**, with the first byte in the stream always having index zero. A 64-bit index is big enough that we can treat it as **never overflowing**. In the TCP headers, however, space is precious, and each byte's index in the stream is represented not with a 64-bit index but with a 32-bit "sequence number," or "seqno." This adds three complexities:

1. **Your implementation needs to plan for 32-bit integers to wrap around.** Streams in TCP can be arbitrarily long -- there's no limit to the length of a ByteStream that can be sent over TCP. But $2^{32}$ bytes is only 4 GiB, which is not so big. Once a 32-bit sequence number counts up to $2^{32} − 1$, the next byte in the stream will have the sequence number zero.
2. **TCP sequence numbers start at a random value:** To improve robustness and avoid getting confused by old segments belonging to earlier connections between the same endpoints, TCP tries to make sure sequence numbers can't be guessed and are unlikely to repeat. So the sequence numbers for a stream don't start at zero. The first sequence number in the stream is a random 32-bit number called the Initial Sequence Number (ISN). This is the sequence number that represents the "zero point" or the SYN (beginning of stream). The rest of the sequence numbers behave normally after that: the first byte of data will have the sequence number of the ISN+1 (mod $2^{32}$), the second byte will have the ISN+2 (mod $2^{32}$), etc.
3. **The logical beginning and ending each occupy one sequence number:** In addition to ensuring the receipt of all bytes of data, TCP makes sure that the beginning and ending of the stream are received reliably. Thus, in TCP the SYN (beginning-of-stream) and FIN (end-of-stream) control flags are assigned sequence numbers. Each of these occupies one sequence number. (The sequence number occupied by the SYN flag is the ISN.) Each byte of data in the stream also occupies one sequence number. Keep in mind that SYN and FIN aren't part of the stream itself and aren't "bytes" -- they represent the beginning and ending of the byte stream itself.

These sequence numbers (**seqnos**) are transmitted in the header of each TCP segment. (And, again, there are two streams -- one in each direction. Each stream has separate sequence numbers and a different random ISN.) It's also sometimes helpful to talk about the concept of an **"absolute sequence number"** (which always starts at zero and doesn't wrap), and about a **"stream index"** (what you've already been using with your Reassembler: an index for each byte in the stream, starting at zero).

To make these distinctions concrete, consider the byte stream containing just the three-letter string *'cat'*. If the SYN happened to have seqno $2^{32}-2$, then the seqnos, absolute seqnos, and stream indices of each byte are:

<div class="center">

| element           | *SYN*      | c          | a | t | *FIN* |
|------------------:|:----------:|:----------:|:-:|:-:|:-----:|
| **seqno**         | $2^{32}-2$ | $2^{32}-1$ | 0 | 1 |  2    |
| **absolute seqno**| 0          | 1          | 2 | 3 |  4    |
| **stream index**  |            | 0          | 1 | 2 |       |

</div>

The figure shows the three different types of indexing involved in TCP:

<div class="center">

Sequence Numbers | Absolute Sequence Numbers | Stream Indices
----------|----------|----------
Start at the ISN | Start at 0 | Start at 0
Include SYN/FIN | Include SYN/FIN | Omit SYN/FIN
32 bits, wrapping | 64 bits, non-wrapping | 64 bits, non-wrapping
 "seqno" | "absolute seqno" | "stream index"

</div>

Converting between absolute sequence numbers and stream indices is easy enough -- just add or subtract one. Unfortunately, converting between sequence numbers and absolute sequence numbers is a bit harder, and confusing the two can produce tricky bugs. To prevent these bugs systematically, we'll represent sequence numbers with a custom type: *Wrap32*, and write the conversions between it and absolute sequence numbers (represented with *uint64_t*). *Wrap32* is an example of a *wrapper type*: a type that contains an inner type (in this case *uint32_t*) but provides a different set of functions/operators.

We've defined the type for you and provided some helper functions (see [wrapping_integers.hh](src/wrapping_integers.hh)), but you'll implement the conversions in [wrapping_integers.cc](src/wrapping_integers.cc):

1. `static Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )`
   
   **Convert absolute seqno → seqno.** Given an absolute sequence number (n) and an Initial Sequence Number (zero_point), produce the (relative) sequence number for n.

2. `uint64_t unwrap( Wrap32 zero_point, uint64_t checkpoint ) const`
   
   **Convert seqno → absolute seqno.** Given a sequence number (the Wrap32), the Initial Sequence Number (zero_point), and an absolute checkpoint sequence number, find the corresponding absolute sequence number that is **closest to the checkpoint**.

   Note: A **checkpoint** is required because any given seqno corresponds to many absolute seqnos. E.g. with an ISN of zero, the seqno "17" corresponds to the absolute seqno of 17, but also $2^{32} + 17$, or $2^{33} + 17$, or $2^{33} + 2^{32} + 17$, or $2^{34} + 17$, or $2^{34} + 2^{32} + 17$, etc. The checkpoint helps resolve the ambiguity: it's an absolute seqno that the user of this class knows is "in the ballpark" of the correct answer. In your TCP implementation, you'll use the first unassembled index as the checkpoint.

   **Hint:** *The cleanest/easiest implementation will use the helper functions provided in [wrapping_integers.hh](src/wrapping_integers.hh). The wrap/unwrap operations should preserve offsets -- two seqnos that differ by 17 will correspond to two absolute seqnos that also differ by 17.*
  
   **Hint #2:** *We're expecting one line of code for wrap, and less than 10 lines of code for unwrap. If you find yourself implementing a lot more than this, it might be wise to step back and try to think of a different strategy.*

## 2 Implementing the TCP receiver
Congratulations on getting the wrapping and unwrapping logic right! We'll shake your hand
(or, post-covid, elbow-bump) if this victory happens at the lab session. In the rest of this
lab, you'll be implementing the TCPReceiver. It will (1) receive messages from its peer's
sender and reassemble the ByteStream using a Reassembler, and (2) send messages back to
the peer's sender that contain the acknowledgment number (ackno) and window size. We're
expecting this to take **about 15 lines of code** in total.

First, let's review the format of a TCP "sender message," which contains the information
about the ByteStream. These messages are sent from a TCPSender to its peer's TCPReceiver:

```cpp
/* The TCPSenderMessage contains four fields (minnow/util/tcp_sender_message.hh):
*
* 1) The sequence number (seqno) of the beginning of the segment. If the SYN flag is set,
* this is the sequence number of the SYN flag. Otherwise, it's the sequence number of
* the beginning of the payload.
*
* 2) The SYN flag. If set, it means this segment is the beginning of the byte stream, and
* that the seqno field contains the Initial Sequence Number (ISN) -- the zero point.
*
* 3) The payload: a substring (possibly empty) of the byte stream.
*
* 4) The FIN flag. If set, it means the payload represents the ending of the byte stream. */

struct TCPSenderMessage
{
  Wrap32 seqno { 0 };
  bool SYN { false };
  Buffer payload {};
  bool FIN { false };

  // How many sequence numbers does this segment use?
  size_t sequence_length() const { return SYN + payload.size() + FIN; }
}
```

The TCPReceiver generates its own messages back to the peer's TCPSender:
```cpp
/* The TCPReceiverMessage structure contains (minnow/util/tcp_receiver_message.hh):
*
* 1) The acknowledgment number (ackno): the *next* sequence number needed by the TCP Receiver.
* This is an optional field that is empty if the TCPReceiver hasn't yet received
* the Initial Sequence Number.
*
* 2) The window size. This is the number of sequence numbers that the TCP receiver is interested
* to receive, starting from the ackno if present. The maximum value is 65,535 (UINT16_MAX from
* the <cstdint> header). */

struct TCPReceiverMessage
{
  std::optional<Wrap32> ackno {};
  uint16_t window_size {};
};
```

Your TCPReceiver's job is to receive one of these kinds of messages and send the other:

```cpp
class TCPReceiver
{
public:
  /* The TCPReceiver receives TCPSenderMessages, inserting their payload
  * into the Reassemble at the correct stream index. */
  void receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream );

  /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
  TCPReceiverMessage send( const Writer& inbound_stream ) const;
};
```

### 2.1 receive()
This is method will be called each time a new segment is received from the peer's sender.
This method needs to:

 - **Set the Initial Sequence Number if necessary.** The sequence number of the first-arriving segment that has the SYN flag set is the *initial sequence number*. You'll want
to keep track of that in order to keep converting between 32-bit wrapped seqnos/acknos
and their absolute equivalents. (Note that the SYN flag is just *one* flag in the header.
The same message could also carry data or have the FIN flag set.)

 - **Push any data to the Reassembler.** If the FIN flag is set in a TCPSegment's header,
that means that the last byte of the payload is the last byte of the entire stream.
Remember that the Reassembler expects stream indexes starting at zero; you will have
to unwrap the seqnos to produce these.
