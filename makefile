all:
	gcc -o ws_fork ws_fork.c
	gcc -o ws_select ws_select.c

clean:
	rm ws_fork ws_select 
