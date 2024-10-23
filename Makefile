debug:
	$(CC) -g -o ./mvi ./mvi.c
release:
	$(CC) -Oz -s -o ./mvi ./mvi.c
clean:
	rm -rf ./mvi
install:release
	sudo cp ./man/mvi.1 /usr/share/man/man1
	sudo mandb
	sudo cp ./mvi /usr/bin/
