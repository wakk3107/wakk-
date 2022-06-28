#include <cstddef>
#include <mutex>
#include <iostream>
//封装了malloc和free操作，可以设置OOM设置内存的回调函数
template <int __inst>
class __malloc_alloc_template
{

private:
    static void *_S_oom_malloc(size_t);
    static void *_S_oom_realloc(void *, size_t);

    static void (*__malloc_alloc_oom_handler)();

public:
    static void *allocate(size_t __n)
    {
        void *__result = malloc(__n);
        if (0 == __result)
            __result = _S_oom_malloc(__n);
        return __result;
    }

    static void deallocate(void *__p, size_t /* __n */)
    {
        free(__p);
    }

    static void *reallocate(void *__p, size_t /* old_sz */, size_t __new_sz)
    {
        void *__result = realloc(__p, __new_sz);
        if (0 == __result)
            __result = _S_oom_realloc(__p, __new_sz);
        return __result;
    }

    static void (*__set_malloc_handler(void (*__f)()))()
    {
        void (*__old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = __f;
        return (__old);
    }
};
template <int __inst>
void (* __malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = nullptr;
template <int __inst>
void *__malloc_alloc_template<__inst>::_S_oom_malloc(size_t __n)
{
    void (*__my_malloc_handler)();
    void *__result;

    for (;;)
    {
        __my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == __my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        (*__my_malloc_handler)();
        __result = malloc(__n);
        if (__result)
            return (__result);
    }
}

template <int __inst>
void *__malloc_alloc_template<__inst>::_S_oom_realloc(void *__p, size_t __n)
{
    void (*__my_malloc_handler)();
    void *__result;

    for (;;)
    {
        __my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == __my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        (*__my_malloc_handler)();
        __result = realloc(__p, __n);
        if (__result)
            return (__result);
    }
}

typedef __malloc_alloc_template<0> malloc_alloc;
enum
{
    _ALIGN = 8 //以八字节开始，以8字节为对齐，不断扩充到128

};
enum
{
    _MAX_BYTES = 128 //内存池最大的chunk块
};
enum
{
    _NFREELISTS = 16 // 自由链表的个数
};
//每一个chunk块的头信息，
union _Obj
{
    union _Obj *_M_free_list_link; //存储下一个chunk块的地址
    char _M_client_data[1];        /* The client sees this.        */
};
template <typename T>
class myallocator
{
private:
    // chunk块的使用情况
    static char *_S_start_free;
    static char *_S_end_free;
    static size_t _S_heap_size;
    //内存池基于freelist实现，需要考虑线程安全
    static std::mutex mtx;
    /*将bytes 上调至最邻近的8的倍数*/
    static size_t _S_round_up(size_t __bytes)
    {
        return (((__bytes) + (size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
    }
    /*返回 bytes 大小的小额区块位于free-list中的编号*/
    static size_t _S_freelist_index(size_t __bytes)
    {
        return (((__bytes) + (size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
    }
    static _Obj *volatile _S_free_list[_NFREELISTS];

public:
    using value_type = T;
    constexpr myallocator(/* args */) noexcept
    {
    }
    constexpr myallocator(const myallocator &) noexcept = default;
    template <typename _Other>
    constexpr myallocator(const myallocator<_Other> &) noexcept
    {
        // donothing
    }
    ~myallocator()=default;

    //开辟内存,传入的类型个数
    T *allocate(size_t __n, const void * = 0)
    {
        __n = __n * sizeof(T);
        void *__ret = 0;

        if (__n > (size_t)_MAX_BYTES)
        {
            __ret = malloc_alloc::allocate(__n);
        }
        else
        {
            _Obj *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);

            std::lock_guard<std::mutex> guard(mtx);

            _Obj *__result = *__my_free_list;
            if (__result == 0)
                __ret = _S_refill(_S_round_up(__n));
            else
            {
                *__my_free_list = __result->_M_free_list_link;
                __ret = __result;
            }
        }

        return (T *)__ret;
    }
    //释放内存，传入的类型个数
    void deallocate(void *__p, size_t __n)
    {
        __n = __n * sizeof(T);
        if (__n > (size_t)_MAX_BYTES)
            malloc_alloc::deallocate(__p, __n);
        else
        {
            _Obj *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);
            _Obj *__q = (_Obj *)__p;

            std::lock_guard<std::mutex> guard(mtx);
            __q->_M_free_list_link = *__my_free_list;
            *__my_free_list = __q;
            // lock is released here
        }
    }

    void *reallocate(void *__p, size_t __old_sz, size_t __new_sz);

    void construct(T *_p, const T &val)
    {
        new (_p) T(val);
    }
    //对象析构
    void destroy(T *_p)
    {
        _p->~T();
    }
    static void *_S_refill(size_t __n)
    {
        int __nobjs = 20;
        char *__chunk = _S_chunk_alloc(__n, __nobjs);
        _Obj *volatile *__my_free_list;
        _Obj *__result;
        _Obj *__current_obj;
        _Obj *__next_obj;
        int __i;

        if (1 == __nobjs)
            return (__chunk);
        __my_free_list = _S_free_list + _S_freelist_index(__n);

        /* Build free list in chunk */
        __result = (_Obj *)__chunk;
        *__my_free_list = __next_obj = (_Obj *)(__chunk + __n);
        for (__i = 1;; __i++)
        {
            __current_obj = __next_obj;
            __next_obj = (_Obj *)((char *)__next_obj + __n);
            if (__nobjs - 1 == __i)
            {
                __current_obj->_M_free_list_link = 0;
                break;
            }
            else
            {
                __current_obj->_M_free_list_link = __next_obj;
            }
        }
        return (__result);
    }
    //主要负责分配自由链表，chunk
    static char *_S_chunk_alloc(size_t __size, int &__nobjs)
    {
        char *__result;
        size_t __total_bytes = __size * __nobjs;
        size_t __bytes_left = _S_end_free - _S_start_free;

        if (__bytes_left >= __total_bytes)
        {
            __result = _S_start_free;
            _S_start_free += __total_bytes;
            return (__result);
        }
        else if (__bytes_left >= __size)
        {
            __nobjs = (int)(__bytes_left / __size);
            __total_bytes = __size * __nobjs;
            __result = _S_start_free;
            _S_start_free += __total_bytes;
            return (__result);
        }
        else
        {
            size_t __bytes_to_get =
                2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
            // Try to make use of the left-over piece.
            if (__bytes_left > 0)
            {
                //剩余的插到能用的那个链表去
                _Obj *volatile *__my_free_list =
                    _S_free_list + _S_freelist_index(__bytes_left);

                ((_Obj *)_S_start_free)->_M_free_list_link = *__my_free_list;
                *__my_free_list = (_Obj *)_S_start_free;
            }
            _S_start_free = (char *)malloc(__bytes_to_get);
            //分配不成功就从大一点chunk力分配一个块当作备用内存池。然后如果该块没有被用完，剩余的byte再次进入这个else也会被上面那个if语句插入到能用的地方
            if (0 == _S_start_free)
            {
                size_t __i;
                _Obj *volatile *__my_free_list;
                _Obj *__p;
                // Try to make do with what we have.  That can't
                // hurt.  We do not try smaller requests, since that tends
                // to result in disaster on multi-process machines.
                for (__i = __size;
                     __i <= (size_t)_MAX_BYTES;
                     __i += (size_t)_ALIGN)
                {
                    __my_free_list = _S_free_list + _S_freelist_index(__i);
                    __p = *__my_free_list;
                    if (0 != __p)
                    {
                        *__my_free_list = __p->_M_free_list_link;
                        _S_start_free = (char *)__p;
                        _S_end_free = _S_start_free + __i;
                        return (_S_chunk_alloc(__size, __nobjs));
                        // Any leftover piece will eventually make it to the
                        // right free list.
                    }
                }
                //若大块的chunk里都没有的话，挣扎一下跳到下面那个函数，再malloc一次，不行就执行用户设置的回调函数，如没有就抛出bad allco
                _S_end_free = 0; // In case of exception.
                _S_start_free = (char *)malloc_alloc::allocate(__bytes_to_get);
            }
            _S_heap_size += __bytes_to_get;
            _S_end_free = _S_start_free + __bytes_to_get;
            return (_S_chunk_alloc(__size, __nobjs));
        }
    }
};
template <typename T>
char *myallocator<T>::_S_start_free = nullptr;

template <typename T>
char *myallocator<T>::_S_end_free = nullptr;

template <typename T>
size_t myallocator<T>::_S_heap_size = 0;

template <typename T>
_Obj *volatile myallocator<T>::_S_free_list[_NFREELISTS] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

template <typename T>
std::mutex myallocator<T>::mtx;