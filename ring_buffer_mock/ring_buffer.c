#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define SIGNAL_LENGTH 75
#define BUFFER_LENGTH 10

typedef struct
{
    uint8_t* current_buffer;
    uint8_t* hold_buffer;
    uint8_t buffer_index;
} RingBuffer;

void next_buffer(RingBuffer* ring_buffer_pointer)
{
    uint8_t* current_buffer_temp = ring_buffer_pointer->current_buffer;
    ring_buffer_pointer->current_buffer = ring_buffer_pointer->hold_buffer;
    ring_buffer_pointer->hold_buffer = current_buffer_temp;
}

void ring_buffer_init(RingBuffer* ring_buffer_pointer)
{
    ring_buffer_pointer->current_buffer = malloc(BUFFER_LENGTH);
    ring_buffer_pointer->hold_buffer = malloc(BUFFER_LENGTH);
    ring_buffer_pointer->buffer_index = 0;
}

void ring_buffer_sample(uint8_t point, RingBuffer* ring_buffer_pointer)
{
    *(ring_buffer_pointer->current_buffer + ring_buffer_pointer->buffer_index) = point;
    ring_buffer_pointer->buffer_index++;
    if(ring_buffer_pointer->buffer_index >= BUFFER_LENGTH)
    {
        next_buffer(ring_buffer_pointer);
        ring_buffer_pointer->buffer_index = 0;
    }
}

void flatten_ring_buffer(RingBuffer* ring_buffer_pointer, uint8_t* flat_buffer)
{
    uint8_t i;
    for(i = 0; i < BUFFER_LENGTH; i++)
    {
        flat_buffer[i+(BUFFER_LENGTH-ring_buffer_pointer->buffer_index)] = ring_buffer_pointer->hold_buffer[i];
        if(i < ring_buffer_pointer->buffer_index) 
            flat_buffer[2*BUFFER_LENGTH-ring_buffer_pointer->buffer_index+i] = ring_buffer_pointer->current_buffer[i];
        else
            flat_buffer[i-ring_buffer_pointer->buffer_index] = ring_buffer_pointer->current_buffer[i];
    }
}

void ring_buffer_teardown(RingBuffer* ring_buffer_pointer)
{
    free(ring_buffer_pointer->current_buffer);
    free(ring_buffer_pointer->hold_buffer);
}

void print_array(uint8_t* array, uint8_t length)
{
    uint8_t i;
    for(i = 0; i < length; i++) printf("%d ", *(array + i));
    printf("\n");
}

int main(void)
{
    RingBuffer ring_buffer;
    ring_buffer_init(&ring_buffer);
    uint8_t i;
    for(i = 0; i < SIGNAL_LENGTH; i++)
    {
        ring_buffer_sample(i, &ring_buffer);
        if(i >= 2*BUFFER_LENGTH - 1)
        {
            uint8_t flattened_buffer[2*BUFFER_LENGTH];
            flatten_ring_buffer(&ring_buffer, flattened_buffer);
            print_array(flattened_buffer, 2*BUFFER_LENGTH);
            uint8_t j;
            for(j=0; j < 2*BUFFER_LENGTH; j++)
            {
                if(j > 0) assert(flattened_buffer[j-1]+1 == flattened_buffer[j]); 
            }
        }
    }
    ring_buffer_teardown(&ring_buffer);
    return 0;
}
