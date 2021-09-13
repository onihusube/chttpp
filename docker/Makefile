build : Dockerfile
	docker build -t chttpenv .

.PHONY: run
run : 
	docker run -it --rm -v=$$PWD/../:/chttpp chttpenv