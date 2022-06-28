//移植nginx内存池的代码，用OOP来实现
#pragma once
#include<cstdlib>
#include<memory>
namespace wakk {

    //类型前置声明
    class wakk_pool_s;
    //类型重定义
    using u_char = unsigned char;
    using wakk_uint_t = unsigned int;
    using wakk_int_t = int;
   
    //清理函数（回调函数）的类型
    typedef void (*wakk_pool_cleanup_pt)(void* data);
    //要用的对齐宏
#define wakk_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define wakk_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define wakk_memzero(buf, n)       (void) memset(buf, 0, n)
    class wakk_pool_cleanup_s {
    public:
        wakk_pool_cleanup_pt handler; //定义了一个函数指针，保存清理操作的回调函数
        void* data;                   //传递给回调函数的参数
        wakk_pool_cleanup_s* next; //所有的cleanup清理操作都被串在一条链表上
    };

    //大块内存的头部信息
    class wakk_pool_large_s {
    public:
        wakk_pool_large_s* next=nullptr; //所有的大块内存分配也是被串在一条链表
        void* alloc = nullptr;             //保存分配出去的大块内存的起始地址
    };

    //分配小块内存的内存池的头部数据信息
    class wakk_pool_data_t {
    public:
        u_char* last;
        u_char* end;
        wakk_pool_s* next;
        wakk_uint_t failed;
    };
    //类型定义
    //默认一个页面大小
    const int wakk_pagesize = 4096;
    //小块内存池可分配的最大内存
    const int WAKK_MAX_ALLOC_FROM_POOL = wakk_pagesize - 1;
    //默认内存池大小
    const int WAKK_DEFAULT_POOL_SIZE = 16 * 1024;
    //默认内存池按照十六字节进行对齐
    const int WAKK_POOL_ALIGNMENT = 16;

    
    //小块内存分配考虑字节对齐时的单位
    const int WAKK_ALIGNMENT = sizeof(unsigned long);
              
    //主内存池的头部信息和管理成员信息
    class wakk_pool_s {
    public:
        wakk_pool_data_t d;
        size_t max;
        wakk_pool_s* current;
        wakk_pool_large_s* large;
        wakk_pool_cleanup_s* cleanup;
    };
    class wakk_mem_pool {
    public:
        wakk_mem_pool() = delete;
        wakk_mem_pool(size_t size){
            wakk_create_pool(size);
        }
        ~wakk_mem_pool() {
            wakk_destroy_pool();
        }
        void* wakk_create_pool(size_t size);
        //考虑内存对齐，从内存池内申请size大小的内存 
        void* wakk_palloc(size_t size);
        //和上面函数一样但不考虑内存对齐
        void* wakk_pnalloc(size_t size);
        //调用的是wakk_palloc，但初始化为0
        void* wakk_pcalloc(size_t size);
        //释放大块内存
        void wakk_pfree(void* p);
        //重置内存
        void wakk_reset_pool();
        //销毁内存池
        void wakk_destroy_pool();
        //添加内存清理回调函数
        wakk_pool_cleanup_s* wakk_pool_cleanup_add(size_t size);
    private:
        //指向内存池的入口指针
        wakk_pool_s* pool_; 
        //小块内存分配
        void* wakk_palloc_small(size_t size, wakk_uint_t align);
        //大块内存分配
        void* wakk_palloc_large(size_t size);
        //分配新的小块内存池
        void* wakk_palloc_block(size_t size);
    };
    const int WAKK_MIN_POOL_SIZE =
        wakk_align((sizeof(wakk_pool_s) + 2 * sizeof(wakk_pool_large_s)),
            WAKK_POOL_ALIGNMENT);
} // namespace wakk
