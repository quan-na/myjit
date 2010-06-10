all: readme.htm 

readme.htm: ../README main.css
	rst2html.py -d -t --embed-stylesheet --stylesheet-path="main.css" ../README > readme.htm
	cp readme.htm ../

web: readme.htm
	mkdir -p ../web
	grep -v "</body>\|</html>" readme.htm > ../web/index.htm
	cat ga.htm >> ../web/index.htm
	echo -e "</body>\n</html>\n" >> ../web/index.htm

clean:
	rm readme.htm
	rm -fr ../web/
