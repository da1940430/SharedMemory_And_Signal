all:prog cnsm

prog:producer.c
	gcc -g producer.c -o prog -D_GNU_SOURCE

cnsm:consumer.c
	gcc -g consumer.c -o cnsm -D_GNU_SOURCE

clean:
	rm prog cnsm