//
//  memory_pool.cpp
//  algorithm
//
//  Created by 韩霄 on 2021/10/27.
//
#include <memory>
#include "memory_pool.hpp"

typedef struct {
//    内存池的开始和结束
    char *last;
    char * end;
    pool_t *next; //指向下一个内存池
    int failed;//失败的次数
}pool_data_t;

struct pool_large_t{
    pool_large_t *next;//指向下一个存储地址。
    void *alloc;  //数据块指针地址
};

struct pool_cleanup_t{
    pool_cleanup_t handler; //清理内存的回调函数
    void *data; //指向存储的数据地址
    pool_cleanup_t *next;//指向下一个清理内存的地址。
};
struct pool_t{
    pool_data_t d;//内存池的数据区域
    size_t max;//最大可分配内存大小
    pool_t  *current;//指向当前的内存池指针地址
    chain_t *chain;//缓冲区链表
    pool_large_t *large;//存储大数据的链表
    pool_clearnup_t *cleanup;//清除内存块的内存。可以自定义回调函数。
    log_t *log;//日志
};

//基于内存池实现的可扩容的数组
typedef struct{
    void *arr_begin;//指向十足第一个元素的指针
    int  n_unused;//未使用的元素索引
    size_t size;//每个元素的大小，元素大小是固定的
    pool_t *pool; //内存池。
}pool_array_t;




void alloc(size_t size,log_t *log){
    void *p;
    p = malloc(size);
    
    if(p ==nullptr){
        log_error("malloc failed",siz);
    }
    return p;
}

void calloc(size_t size,log_t *log){
    void *p;
    p = alloc(size,log);
    if(p)
        memzero(p,size);// 指定的内存块全部设置为0
    
    return p;
}

pool_t create_poll(size_t size,log_t *log){
    pool_t *p;
    
    p = memalign(size,log);
    if(p == NULL) return NULL;
    else{
//        分配成功
        /**
             * Nginx会分配一块大内存，其中内存头部存放ngx_pool_t本身内存池的数据结构
             * ngx_pool_data_t    p->d 存放内存池的数据部分（适合小于p->max的内存块存储）
             * p->large 存放大内存块列表
             * p->cleanup 存放可以被回调函数清理的内存块（该内存块不一定会在内存池上面分配）
             */
            p->d.last = (u_char *) p + sizeof(ngx_pool_t); //内存开始地址，指向ngx_pool_t结构体之后数据取起始位置
            p->d.end = (u_char *) p + size; //内存结束地址
            p->d.next = NULL; //下一个ngx_pool_t 内存池地址
            p->d.failed = 0; //失败次数
         
            size = size - sizeof(ngx_pool_t);
            p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;
         
            /* 只有缓存池的父节点，才会用到下面的这些  ，子节点只挂载在p->d.next,并且只负责p->d的数据内容*/
            p->current = p;
            p->chain = NULL;
            p->large = NULL;
            p->cleanup = NULL;
            p->log = log;
        
    }
    
    return p;
}

//销毁内存池
void destroy_pool(pool_t *pool){
    pool_t *p, *n;
    pool_large_t *l;
    pool_cleanup_t *c;
    /* 首先清理pool->cleanup链表 */
        for (c = pool->cleanup; c; c = c->next) {
            /* handler 为一个清理的回调函数 */
            if (c->handler) {
                ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                        "run cleanup: %p", c);
                c->handler(c->data);
            }
        }
     
        /* 清理pool->large链表（pool->large为单独的大数据内存块）  */
        for (l = pool->large; l; l = l->next) {
     
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc);
     
            if (l->alloc) {
                ngx_free(l->alloc);
            }
        }

//    释放内存池的data数据区域
    for(p = poll,n = poll->d.next;p = n;n = n->next){
        free(p);
        if(n == NULL) break;
    }
  }

//重设内存池
void ngx_reset_pool(ngx_pool_t *pool) {
    pool_t *p;
    pool_large_t *l;
//    清理pool->large链表。
    for(l = pool->large;l;l = l->next){
        if(l->alloc){
            free(l->alloc);
        }
    }
    pool->large = NULL;
//    循环重新设置内存池的data区域的p->data.last
    for(p = pool;p;p = p->d.next){
        p->d.last = (char*) p + sizeof(pool_t);
    }
}

//使用内存池分配一块内存，返回void指针。
void palloc(pool_t *pool,size_t size){
    char* m;
    pool_t *p;
    
//    判断每次分配的内存大小。如果超出限制，则要走大数据块内存分配
    if(size <pool->max){
        p = pool->current;
        /*
                 * 循环读取缓存池链p->d.next的各个的ngx_pool_t节点，
                 * 如果剩余的空间可以容纳size，则返回指针地址
                 *
                 * 这边的循环，实际上最多只有4次，具体可以看ngx_palloc_block函数
                 * */
                do {
                    /* 对齐操作,会损失内存，但是提高内存使用速度 */
                    m = ngx_align_ptr(p->d.last, NGX_ALIGNMENT);
         
                    if ((size_t)(p->d.end - m) >= size) {
                        p->d.last = m + size;
         
                        return m;
                    }
         
                    p = p->d.next;
         
                } while (p);
//        如果找不到缓存池空间能容纳size的，就重新申请内存
        return palloc_block(pool, size);
        }
//    否则，走大数据块分配策略。在pool->large 链表上分配。
    return palloc_large(pool,size);
}

//使用内存池分配一块内存，(不考虑内存对齐)返回void指针。
void pnalloc(pool_t *pool,size_t size){
    p = pool->current;
    /*
             * 循环读取缓存池链p->d.next的各个的ngx_pool_t节点，
             * 如果剩余的空间可以容纳size，则返回指针地址
             *
             * 这边的循环，实际上最多只有4次，具体可以看ngx_palloc_block函数
             * */
            do {
//                与上面不同之处是没有这句内存对齐。
//                /* 对齐操作,会损失内存，但是提高内存使用速度 */
//                m = ngx_align_ptr(p->d.last, NGX_ALIGNMENT);
     
                if ((size_t)(p->d.end - m) >= size) {
                    p->d.last = m + size;
     
                    return m;
                }
     
                p = p->d.next;
     
            } while (p);
//        如果找不到缓存池空间能容纳size的，就重新申请内存
    return palloc_block(pool, size);
    }
//    否则，走大数据块分配策略。在pool->large 链表上分配。
return palloc_large(pool,size);
}

//内存池扩容
static void* palloc_block(pool_t *pool,size_t size){
        char *m;
        size_t psize;
        ngx_pool_t *p, *new, *current;
//     当前内存池的大小
        psize = (size_t)(pool->d.end - (u_char *) pool);
     
        /* 申请新的块 */
        m = memalign(NGX_POOL_ALIGNMENT, psize, pool->log);
        if (m == NULL) {
            return NULL;
        }
     
        new = (ngx_pool_t *) m;
     
        new->d.end = m + psize;
        new->d.next = NULL;
        new->d.failed = 0;
//    分配size大小的内存块，返回m指针地址
    m +=sizeof(pool_data_t);
    m = align_ptr(m,ALIGNMENT);
    new->d.last = m+size;
    current = pool->current;
    /**
         * 缓存池的pool数据结构会挂载子节点的ngx_pool_t数据结构
         * 子节点的ngx_pool_t数据结构中只用到pool->d的结构，只保存数据
         * 每添加一个子节点，p->d.failed就会+1，当添加超过4个子节点的时候，
         * pool->current会指向到最新的子节点地址
         *
         * 这个逻辑主要是为了防止pool上的子节点过多，导致每次ngx_palloc循环pool->d.next链表
         * 将pool->current设置成最新的子节点之后，每次最大循环4次，不会去遍历整个缓存池链表
         */
        for (p = current; p->d.next; p = p->d.next) {
            if (p->d.failed++ > 4) {
                current = p->d.next;
            }
        }
     
        p->d.next = new;
     
        /* 最终这个还是没变 */
        pool->current = current ? current : new;
     
        return m;

}
//分配一块大内存。挂载到pool->large 链表上palloc_large
static void* palloc_large(pool_t *pool,size_t size){
        void *p;
        int_t n;
        pool_large_t *large;
     
        /* 分配一块新的大内存块 */
        p = ngx_alloc(size, pool->log);
        if (p == NULL) {
            return NULL;
        }
     
        n = 0;
    /* 去pool->large链表上查询是否有NULL的，只在链表上往下查询3次，主要判断大数据块是否有被释放的，如果没有则只能跳出*/
        for (large = pool->large; large; large = large->next) {
            if (large->alloc == NULL) {
                large->alloc = p;
                return p;
            }
     
            if (n++ > 3) {
                break;
            }
        }
    large = ngx_palloc(pool, sizeof(ngx_pool_large_t));
        if (large == NULL) {
            ngx_free(p); //如果分配失败，删除内存块
            return NULL;
        }
     
        large->alloc = p;
        large->next = pool->large;
        pool->large = large;
     
        return p;
}
  //大内存块的释放pfree
int pfree(pool_t *pool,void *p){
    pool_large_t *l;
    
//    在pool->large 链上循环搜索，并且只是放内容数据。不释放数据结构
    for(l =pool->large;l;l=l->next){
        if(p == l->alloc){
            log();
            free(l->alloc);
            l->alloc =NULL;
            return OK;
        }
    }
    return -1;
}












