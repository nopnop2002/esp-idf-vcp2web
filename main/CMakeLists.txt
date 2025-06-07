idf_component_register(SRCS "main.c" "web_server.c" "web_client.c" "cdc_acm_vcp.cpp"
	INCLUDE_DIRS "."
	EMBED_FILES "../html/error.html"
		"../html/favicon.ico"
		"../html/main.js"
		"../html/root.html"
	REQUIRED_IDF_TARGETS esp32s2 esp32s3)
