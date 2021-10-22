这个lab一开始有点难以下手，几个成员变量类都是之前没用过的。  
先看看初始模板都给了什么。   
### _segments_out
这是Sender内部维护一个队列。我们需要把发的东西放进队列里面。需要注意的是，和STL的队列不同，这里的队列要先push，然后取它的引用来赋值（而不是先构造一个对象再push).
#### TCPHeader
header对象就是TCP的头部报文。这里我们主要要修改的是各种FLAG和Seq。对于校验和，端口之类的不需要管。使用的时候，这样写就可以:
```cpp
segment s;
s.header().syn = true;
```

#### TCPpayload
这个Payload我一开始以为要用parse才能赋值，后来测试的时候发现赋不上值。后面搜了sponge内部使用TCPSegment的代码发现，它是直接赋值的，因为这里重载了`string_view`
```cpp
TCPSegment seg;
seg.payload() = std::string(data); // data也是string类型
```
注意右边必须要是一个右值引用（将亡值），如果直接写data会报错。
关于string_view，我也不太了解，后面补上
### _stream
这是输入的流。注意它不是一直有值的，也可能是慢慢的输入的。当输入结束，产生EOF的时候，可以通过_stream.eof()判断（不要用_stream.empty()去判断结束）

## 实现
### 大致还需要的变量
* 发生方窗口
* 超时重传时钟
	内部可能需要记录
	* 当前计时
	* seq
	* 初始rto
	* 超时重传次数
	* 上次传输的seq，标志和字符串（比如eof)
	* 计时是否有效，是否过期
### fill_window函数
这个函数需要按窗口大小填充segment队列。基本的操作就是向队列压一个对象，修改标记，添加数据，开启定时器。
几个可能需要注意的点：
* 如果数据包为空，不能允许发送报文出去
	* 但是如果数据为空，且是SYN（第一个握手）则允许发送该报文
* SYN报文不允许发送FIN标记，SYN报文也不能携带数据，SYN占1个位
* 要在这里设置`_next_seqno`。`_next_seqno`取决发送数据包大小而不是窗口大小
### ack_received
这个函数是ack到达时的回调函数，需要负责处理窗口和关闭定时器。
可能需要注意的问题：
* 需要判断ACKno是否有效。若上一次发送是seq,上一次发送的数据是n，有效范围应该是$ seq \lt ackno \le seq+n$。注意等号
	* 如果是syn报文。n应该取1
* 窗口大小要设成接收方窗口大小。如果是0要调成1

### bytes_in_flight
这里我做了一个条件判断，如果时钟正在运行，则返回next_seq减去上一次seq；否则，返回一个0


# send_transmit
* 时钟要有多个
* ack要一次消除多个时钟
* 用链表

# send_window
* 窗口每次都要更新，不能有效才更新
* FIN也占窗口，如果窗口已满，要下次传

# extra
* MAX_PAYLOAD_SIZE limits payload only
* fill_window() correctly fills a big window
* When filling window, treat