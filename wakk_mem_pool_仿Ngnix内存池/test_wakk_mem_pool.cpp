#include"wakk_mem_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace wakk;
typedef struct Data stData;
struct Data
{
    char* ptr;
    FILE* pfile;
};

void func1(void* p1)
{
    char* p = (char*)p1;
    printf("free ptr mem!");
    free(p);
}
void func2(void* pf1)
{
    FILE* pf = (FILE*)pf1;
    printf("close file!");
    fclose(pf);
}
int main()
{
    // 512 - sizeof(ngx_pool_t) - 4095   =>   max
    wakk_mem_pool* pool = new wakk_mem_pool(512);
    if (pool == nullptr)
    {
        printf("ngx_create_pool fail...");
        return 0;
    }

    void* p1 = pool->wakk_palloc(128); // ��С���ڴ�ط����
    if (p1 == nullptr)
    {
        printf("ngx_palloc 128 bytes fail...");
        return 0;
    }

    stData* p2 = (stData*)pool->wakk_palloc(512); // �Ӵ���ڴ�ط����
    if (p2 == nullptr)
    {
        printf("ngx_palloc 512 bytes fail...");
        return 0;
    }
    p2->ptr =(char*)malloc(12);
    strcpy(p2->ptr, "hello world");
    p2->pfile = fopen("data.txt", "w");

    wakk_pool_cleanup_s* c1 = pool->wakk_pool_cleanup_add(sizeof(char*));
    c1->handler = (wakk_pool_cleanup_pt)func1;
    c1->data = p2->ptr;

    wakk_pool_cleanup_s* c2 = pool->wakk_pool_cleanup_add(sizeof(FILE*));
    c2->handler = (wakk_pool_cleanup_pt)func2;
    c2->data = p2->pfile;

            //�������� 1.�������е�Ԥ�õ������� 2.�ͷŴ���ڴ� 3.�ͷ�С���ڴ�������ڴ�
    delete(pool);
    return 0;
}
