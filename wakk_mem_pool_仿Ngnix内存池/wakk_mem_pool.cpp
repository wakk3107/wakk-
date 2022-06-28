#include "wakk_mem_pool.h"

namespace wakk {
	void* wakk_mem_pool::wakk_create_pool(size_t size) {
		wakk_pool_s* pool_;
        pool_ = (wakk_pool_s*)malloc(size);
        if (pool_ == nullptr) {
            return false;
        }
        pool_->d.last = (u_char*)pool_ + sizeof(wakk_pool_s);
        pool_->d.end = (u_char*)pool_ + size;
        pool_->d.next = nullptr;
        pool_->d.failed = 0;

        size = size - sizeof(wakk_pool_s);
        pool_->max = (size < WAKK_MAX_ALLOC_FROM_POOL) ? size : WAKK_MAX_ALLOC_FROM_POOL;

        pool_->current = pool_;
        pool_->large = nullptr;
        pool_->cleanup = nullptr;
        this->pool_ = pool_;
        return pool_;
	}
	void* wakk_mem_pool::wakk_palloc(size_t size) {
		if (size <= pool_->max) {
			return wakk_palloc_small(size, 1);
		}

		return wakk_palloc_large(size);
	}
    //和上面函数一样但不考虑内存对齐
    void* wakk_mem_pool::wakk_pnalloc(size_t size) {
        if (size <= pool_->max) {
            return wakk_palloc_small(size, 0);
        }

        return wakk_palloc_large(size);
    }
    //调用的是wakk_palloc，但初始化为0
    void* wakk_mem_pool::wakk_pcalloc(size_t size) {
        void* p;
        p = wakk_palloc(size);
        if (p) {
            wakk_memzero(p, size);
        }

        return p;
    }
    //小块内存分配
    void* wakk_mem_pool::wakk_palloc_small(size_t size, wakk_uint_t align) {
        u_char* m;
        wakk_pool_s* p;

        p = pool_->current;

        do {
            m = p->d.last;

            if (align) {
                m = wakk_align_ptr(m, WAKK_ALIGNMENT);
            }

            if ((size_t)(p->d.end - m) >= size) {
                p->d.last = m + size;

                return m;
            }

            p = p->d.next;

        } while (p);

        return wakk_palloc_block(size);
    }
    //大块内存分配
    void* wakk_mem_pool::wakk_palloc_large(size_t size) {
        void* p;
        wakk_uint_t         n;
        wakk_pool_large_s* large;

        p = malloc(size);
        if (p == nullptr) {
            return nullptr;
        }

        n = 0;

        for (large = pool_->large; large; large = large->next) {
            if (large->alloc == nullptr) {
                large->alloc = p;
                return p;
            }

            if (n++ > 3) {
                break;
            }
        }

        large = (wakk_pool_large_s*)( wakk_palloc_small(sizeof(wakk_pool_large_s), 1) );
        //large = (wakk_pool_large_s*)malloc(sizeof(wakk_pool_large_s));
        if (large == nullptr) {
            free(p);
            return nullptr;
        }
        memset(large, 0, sizeof(wakk_pool_large_s));
        large->alloc = p;
        large->next = pool_->large;
        pool_->large = large;

        return p;
    }
    //分配新的小块内存池
    void* wakk_mem_pool::wakk_palloc_block(size_t size) {
        u_char* m;
        size_t       psize;
        wakk_pool_s* p, * new_pool;

        psize = (size_t)(pool_->d.end - (u_char*)pool_);

        m = (u_char*)malloc(psize);
        if (m == nullptr) {
            return nullptr;
        }

        new_pool = (wakk_pool_s*)m;

        new_pool->d.end = m + psize;
        new_pool->d.next = nullptr;
        new_pool->d.failed = 0;

        m += sizeof(wakk_pool_data_t);
        m = wakk_align_ptr(m, WAKK_ALIGNMENT);
        new_pool->d.last = m + size;

        for (p = pool_->current; p->d.next; p = p->d.next) {
            if (p->d.failed++ > 4) {
                pool_->current = p->d.next;
            }
        }

        p->d.next = new_pool;

        return m;
    }
    //释放大块内存
    void wakk_mem_pool::wakk_pfree(void* p) {
        wakk_pool_large_s* l;

        for (l = pool_->large; l; l = l->next) {
            if (p == l->alloc) {       
                free(l->alloc);
                l->alloc = nullptr;
                return;
            }
        }
    }
    //重置内存
    void wakk_mem_pool::wakk_reset_pool() {
        wakk_pool_s* p;
        wakk_pool_large_s* l;

        for (l = pool_->large; l; l = l->next) {
            if (l->alloc) {
                free(l->alloc);
            }
        }
        //处理第一块内存池
        p = pool_;
        p->d.last = (u_char*)p + sizeof(wakk_pool_s);
        p->d.failed = 0;
        //第二块内存池开始循环到最后一个内存池，因为第一块内存池的结构的东西多一点，所以先弄
        for (p = pool_->d.next; p; p = p->d.next) {
            p->d.last = (u_char*)p + sizeof(wakk_pool_data_t);
            p->d.failed = 0;
        }

        pool_->current = pool_;
        pool_->large = nullptr;
    }
    //销毁内存池
    void wakk_mem_pool::wakk_destroy_pool() {
        wakk_pool_s* p, * n;
        wakk_pool_large_s* l;
        wakk_pool_cleanup_s* c;

        for (c = pool_->cleanup; c; c = c->next) {
            if (c->handler) {
                c->handler(c->data);
            }
        }

        for (l = pool_->large; l; l = l->next) {
            if (l->alloc) {
                free(l->alloc);
            }
        }

        for (p = pool_, n = pool_->d.next; /* void */; p = n, n = n->d.next) {
            free(p);

            if (n == nullptr) {
                break;
            }
        }
    }
    wakk_pool_cleanup_s* wakk_mem_pool::wakk_pool_cleanup_add(size_t size) {
        wakk_pool_cleanup_s* c;

        c = (wakk_pool_cleanup_s*)wakk_palloc(sizeof(wakk_pool_cleanup_s));
        if (c == nullptr) {
            return nullptr;
        }

        if (size) {
            c->data = wakk_palloc(size);
            if (c->data == nullptr) {
                return nullptr;
            }

        }
        else {
            c->data = nullptr;
        }

        c->handler = nullptr;
        c->next = pool_->cleanup;

        pool_->cleanup = c;

        return c;
    }

 }

