FLAGS=-Wall -Werror -Wpedantic -g #-DVERBOSE

naive_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c src/$@.c -I include
	./bin/$@
	
true_malloc:
	gcc $(FLAGS) -o bin/$@ test/malloc.t.c
	./bin/$@

clean:
	rm bin/*