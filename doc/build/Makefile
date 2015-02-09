all: readme.htm  ga.htm sflogo.htm 

readme.htm: ../README main.css
	#pygmentize -S default -f html > final-style.css
#	pygmentize -S default -f html > final-style.css
	cat main.css code-style.css >> final-style.css
	rst2html -d -t --embed-stylesheet --stylesheet-path="final-style.css" ../README > readme.htm
	cp readme.htm ../

web: readme.htm
	mkdir -p ../web
	grep -v "</body>\|</html>" readme.htm > ../web/index.htm
	cat ga.htm >> ../web/index.htm
	cat sflogo.htm >> ../web/index.htm
	echo -e "</body>\n</html>\n" >> ../web/index.htm

clean:
	rm readme.htm
	rm -f final-style.css
	rm -fr ../web/
