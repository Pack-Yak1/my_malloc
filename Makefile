FLAGS=-g -Wall -Werror -Wpedantic -Wno-unused-result -O3 #-DVERBOSE

brk_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include

mmap_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include
	
true_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c

clean:
	rm bin/*