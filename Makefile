FLAGS=-g -Wall -Werror -Wpedantic #-DVERBOSE

brk_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include

mmap_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include
	
true_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c

clean:
	rm bin/*