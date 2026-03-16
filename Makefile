all:
	gcc -g *.c mail/*.c -o main.exe
run:
	./main.exe

clean:
	rm *.exe *.dll *.o *.a
