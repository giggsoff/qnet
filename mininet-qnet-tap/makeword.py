#!/usr/bin/env python
# -*- coding: utf-8 -*-
from docx import Document
import os
import sys


def process(document, line):
    document.add_paragraph(unicode(line, "utf-8"))


filename = 'demo.docx'

if not os.path.exists(filename):
    document = Document()
else:
    document = Document(filename)

while True:
    line = sys.stdin.readline()
    if not line:
        break
    process(document, line.rstrip())
"""
document.add_heading('Document Title', 1)

p = document.add_paragraph('A plain paragraph having some ')
p.add_run('bold').bold = True
p.add_run(' and some ')
p.add_run(u' перенос ')
p.add_run('italic.').italic = True

document.add_heading('Heading, level 1', level=1)
document.add_paragraph('Intense quote', style='IntenseQuote')

document.add_paragraph(
    'first item in unordered list', style='ListBullet'
)
document.add_paragraph(
    'first item in ordered list', style='ListNumber'
)

# document.add_page_break()
"""
document.save(filename)

