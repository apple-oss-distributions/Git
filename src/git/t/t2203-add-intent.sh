	git add elif
	test $(git diff --name-only HEAD -- nitfol | wc -l) = 1
	git diff --cached --name-only >actual &&
	git diff --cached --name-only >actual &&
