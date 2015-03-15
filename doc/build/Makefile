all: README html web pdf

README: src/*
	cat src/01-About src/03-Instruction-set src/04-Getting-started src/05-Debugging src/06-Optimizations src/02-Download src/07-Notes > ../README

html: README assets/*.css
	cat assets/main.css assets/code-style.css > assets/final-style.css
	rst2html -d -t --embed-stylesheet --stylesheet-path="assets/final-style.css" ../README > readme.htm
	cp readme.htm ../

web: src/* assets/*.css assets/template.htm
	mkdir -p web
	cat assets/web-main.css assets/code-style.css > assets/final-style.css
	cd src; \
	for a in *; do \
		echo $$a; \
		rst2html --embed-stylesheet --stylesheet-path="../assets/final-style.css" --template="../assets/template.htm" $$a | grep -v "</body>\|</html>" > ../web/$$a.htm; \
		echo -e "</body>\n</html>\n" >> ../web/$$a.htm; \
	done 
	cat web/01-About.htm | sed -e 's/<h1 class="title">/<h1 class="title">About /' > web/index.htm

pdf: README assets/xetex-template.tex
	mkdir -p latex
	rst2xetex --template=assets/xetex-template.tex --use-verbatim-when-possible ../README | sed -e 's/section{MyJIT/section{About MyJIT/' >  latex/readme.tex
	cd latex; xelatex readme.tex; xelatex readme.tex
	cp latex/readme.pdf ../

clean:
	rm -f README
	rm -f readme.htm
	rm -f assets/final-style.css
	rm -fr web
	rm -fr latex

