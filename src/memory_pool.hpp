//
//  memory_pool.hpp
//  algorithm
//
//  Created by 韩霄 on 2021/10/27.
//

#ifndef memory_pool_hpp
#define memory_pool_hpp

#include <stdio.h>
void alloc(size_t size,log_t *log);
void calloc(size_t size,log_t *log);
pool_t create_poll(size_t size,log_t *log);
void destroy_pool(pool_t *pool);
void ngx_reset_pool(ngx_pool_t *pool);
void palloc(pool_t *pool,size_t size);
void pnalloc(pool_t *pool,size_t size);

static void* palloc_block(pool_t *pool,size_t size);
static void* palloc_large(pool_t *pool,size_t size)
#endif /* memory_pool_hpp */
