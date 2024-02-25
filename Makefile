FLAGS=-g -Wall -Werror -Wpedantic -Wno-unused-result -O3 -pthread #-DVERBOSE

brk_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include

mmap_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include
	
true_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c

smam:
	gcc $(FLAGS) -o bin/$@ test/arena_manager.t.c src/single_mutex_arena_manager.c -I include

clean:
	rm bin/*