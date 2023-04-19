FILE=log-rcu-$(shell date +"%Y-%m-%d")
LOG_FILE=logs/$(FILE)

urcu:
	gcc -o test test_urcu.c -DDYNAMIC_LINK_TEST -lurcu

mthpc:
	gcc -o test ../../libmthpc.so test_urcu.c -I../../include -DDYNAMIC_LINK_TEST -DUSE_MTHPC_LIB -lurcu

run:
	./test 100 1 10

clean:
	rm -f test

gen-log:
	touch $(LOG_FILE)
