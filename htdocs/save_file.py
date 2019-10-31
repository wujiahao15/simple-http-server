import cgi
import os
import io
import sys
import cgitb
cgitb.enable()
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
form = cgi.FieldStorage()

# get filename
fileitem = form['filename']

# detect whether the file is uploaded
if fileitem.filename:
    filename = os.path.basename(fileitem.filename)
    open('/tmp/' + filename, 'wb').write(fileitem.file.read())

    message = "file {:s} uploaded.".format(filename)

else:
    message = 'The file has not been uploaded.'

print("""Content-Type: text/html\r\n
    <html>
    <head>
    <meta charset="utf-8">
    <title>菜鸟教程(runoob.com)</title>
    </head>
    <body>
    <p>{:s}</p>
    </body>
    </html>""".format(message))
