#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

typedef char ALIGN[16];

/**
 * 使用union的目的是为了字节对齐，因为union结构的大小就是内部最大成员的大小
 * 使用链表结构来追踪内存
 * 开辟内存很容易，但是释放内存就需要知道释放哪片内存，这片内存是多大，所以要使用header来跟踪
 * */
union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub;
};
typedef union header header_t;

// 头尾节点
header_t  *head, *tail;
// 锁，防止多线程操作内存
pthread_mutex_t global_malloc_lock;

header_t *get_free_block(size_t size) {
    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) {
            return curr;
        }
        curr = curr->s.next;
    }
    return NULL;
}

void *malloc(size_t size) {
    size_t total_size;
    void *block;
    header_t  *header;
    if (!size) {
        return NULL;
    }
    // 加锁
    pthread_mutex_lock(&global_malloc_lock);
    header = get_free_block(size);
    if (header) {
        header->s.size = size;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(header+1);
    }
    total_size = sizeof(header_t) + size;
    block = sbrk(total_size);
    if (block == (void *)-1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;
    if (!head) {
        head = header;
    }
    if (tail) {
        tail->s.next = header;
    }
    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1);
}

void free(void *block) {
    header_t *header, *tmp;
    void *programbreak;

    if (!block) {
        return;
    }
    pthread_mutex_lock(&global_malloc_lock);
    header = (header_t *)block - 1;
    // 找到当前已用内存的末尾
    programbreak = sbrk(0);
    // block首地址 + block的长度如果等于末尾的话，则说明是最后一个区块
    if ((char *)block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            tmp = head;
            // 更新链表，把尾节点向前移动
            while (tmp) {
                if (tmp->s.next == tail) {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        // 向前释放内存，释放内存大小为 header长度 + block长度
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

int main() {
    printf("Hello, World!\n");
    return 0;
}
