#include "stream_reassembler.hh"
#include<queue>
#include<string>
#include<cstring>
#include<map>
#include<utility>

// #define DEBUG

#include<iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//constexpr int MAX_DATAGRAM_LENGTH=1500;
StreamReassembler::StreamReassembler(const size_t _capacity) : 
    _output(_capacity),capacity(_capacity),
//    first_unread(0),
    first_unassembled(0),
    _unassembled_bytes(0),
    _eof(false),
    _eof_pos(0),
    blocks{},
    _buffer(new char[_capacity+2])
    {

        // buffer.reserve(capacity);
        // memset(_buffer,0,sizeof(_buffer));
    }

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t size = data.size();
    size_t first_unacceptable = _output.bytes_read() + capacity;
    if (index+size < first_unassembled || index>first_unacceptable){// 在区间之外的两种情况
        return;
    } 
    if(size ==0){
        if(eof && blocks.empty() && index==first_unassembled){//特判
            // cerr<<"index:"<<index<<"\n";
            // cerr<<"capacity:"<<capacity<<"\nfirst_unassembled:"<<first_unassembled<<"\nfirst_unacceptable: "<<first_unacceptable<<endl;
            // cerr<<"end_input222222"<<endl;
            _output.end_input();
        }
        return;
    }
    // 裁剪片段，去掉多余的头和不能接受的尾
    // 头如果在前面，那么移到first_unassembled
    size_t origin_start;
    size_t copy_start;
    size_t copy_end;
    if(index < first_unassembled){
        copy_start = first_unassembled;
        origin_start = first_unassembled - index;
    }else{
        copy_start = index;
        origin_start = 0;
    }
    copy_end = (index+size<=first_unacceptable)?index + size:first_unacceptable;//开区间
    auto copy_size = copy_end - copy_start;
    // origin_end = origin_start + copy_size - 1;
    // cout<<copy_start << " "<< copy_end<<" origin:"<<origin_start<<" " <<capacity<<endl;
    if(eof && index+size<=first_unacceptable){ //处理eof，如果eof在外面，那么认为没收到eof
        _eof = true;
        _eof_pos = copy_end-1;
    }
    #ifdef DEBUG
    size_t pre_first_unassembled = first_unassembled;
    #endif
    if (copy_size>0){
        // 新到的覆盖旧的
        if( copy_start % capacity + copy_size  >= capacity){
            //需要分成两段复制
            // printf("size1:%d\n",size1);
            int size1 = capacity - copy_start % capacity;
            int size2 = copy_size  - size1;
            memcpy(&_buffer[copy_start % capacity],data.c_str()+origin_start,size1);
            memcpy(&_buffer[0],data.c_str()+origin_start+size1,size2);
        }else{
            memcpy(&_buffer[copy_start % capacity],data.c_str()+origin_start,copy_size);// 不复制\0
        }   
        // 合并确定窗口
        add(copy_start,copy_end);
        if(_eof && first_unassembled>_eof_pos){
            cerr<<"end input!!!!!!!!!!!!!!!!!"<<endl;
            _output.end_input();
        }
    }
    // cerr<<"\033[34mfirst_unassembled"<<dec<<first_unassembled<<"\033[0m\n";
    #ifdef DEBUG
    cout<<"\033[34m[pre_first_unassembled] "<<pre_first_unassembled<<" [after_first_unassembled]: "<<first_unassembled<<
    "[first_unacceptable] "<<first_unacceptable<<
    " [eof parameter]="<<eof<<" [eof_pos]"<<_eof_pos<<" [capacity]"<<capacity<<
    "\n[index]"<<index<<" [size] "<<size<< "[copy_start] "<<copy_start<<"[copy_end]"<<copy_end<<"[copy_size] "<<copy_size<<
    "\n[receive data]: "<<data<<"[actual data]"<<data.substr(origin_start,copy_size)<<"[index] "<<index<<
    "\n[_output.buffer_size]"<<_output.buffer_size()<<
    "\n[_unassembled_bytes]"<<_unassembled_bytes<<
    "\033[0m\n"<<endl;
    #endif
    // static size_t a=0;
    // a+=_output.buffer_size();
    // cout<<a<<endl;
}

size_t StreamReassembler::unassembled_bytes() const {
    return static_cast<size_t>(_unassembled_bytes);
 }

bool StreamReassembler::empty() const { 
    return _unassembled_bytes==0;
}


/**
 * @brief 合并窗口
 * 
 * @param left 左区间
 * @param right 右区间
 */
size_t StreamReassembler::add(size_t left, size_t right){
    if(left>right)return 0;
   if (!blocks.empty()){
    //找到第一个
    auto current = blocks.upper_bound(left);// 找到第一个大于left的值
    if (current != blocks.begin())--current;// 如果left不是第一个元素

    if (current->second.second < left) // 如果之前这个元素和新区间毫无相交，那么跳过
        ++current;

    if (current != blocks.end() && current->first <= right) // 确定新的左区间，如果在end处，说明新区间和之前没有重叠
        left = min(left, current->first);

    for (; current != blocks.end() && current->first <= right;
           current = blocks.erase(current)){
        right = max(right, current->second.second);// 合并新的右区间
        // cout<<"合并新的区间"<<current->first<<" "<<current->second.second<<endl;
        _unassembled_bytes -= current->second.second - current->second.first;// 移除旧的
    }
   }

  if(left == first_unassembled){
    // _buffer[right-first_unassembled] = '\0';
    int n = right - first_unassembled;
    vector<char> s;
    s.resize(n);
    for(int i=0,j=first_unassembled % capacity;i<n;i++,j++){
        if(j==static_cast<int>(capacity))j=0;
        s[i] = _buffer[j];
        // cout<<static_cast<int>(_buffer[j])<<endl;
    }
    // cout<<s.data();
    _output.write(string(s.begin(),s.end()));
    first_unassembled = right;
  }else{
    blocks[left] = make_pair(left, right);// 插入新区间
    _unassembled_bytes +=  right - left;
  }
  #ifdef DEBUG
  cout<<"\033[34m insert ["<<left<<","<<right<<")\nset:";
  for(auto val :blocks ){
        cout<<"["<<val.second.first<<","<<val.second.second<<"],";
    }
    cout<<"\033[0m";
    cout<<endl;
#endif
  return first_unassembled;
}
