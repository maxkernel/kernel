#!/usr/bin/python

import sys, time

print >> sys.stdout, '''
<html>
	<head>
		<title>MaxKernel API Documentation</title>
		<!-- Generated %(datetime)s -->
	</head>
	<body>
		<h1>MaxKernel API Documentation</h1>
		<h3>Generated </h3>
		<ul>
			<li>Client Library (libmax)</li>
			<ul>
				<li><a href="libmax/purejava/html/index.html">Java</a></li>
			</ul>
		</ul>
	</body>
</html>

''' % {
'datetime'      :    time.asctime(),
}

