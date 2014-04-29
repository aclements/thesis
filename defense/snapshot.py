# Requires python-webkit-dev

import sys, os, shutil, json

import pygtk
pygtk.require('2.0')
import gtk
import webkit
import SimpleHTTPServer
import SocketServer
import threading

info = []

def main():
    for dirs in ["thumbs", "thumbs-svg"]:
        shutil.rmtree(dirs, ignore_errors=True)
        os.mkdir(dirs)

    handler = SimpleHTTPServer.SimpleHTTPRequestHandler
    httpd = SocketServer.TCPServer(("", 0), handler)
    port = httpd.server_address[1]

    thread = threading.Thread(target=httpd.serve_forever)
    thread.daemon = True
    thread.start()

    window = gtk.OffscreenWindow()
    view = webkit.WebView()
    # XXX Get from Javascript, which knows SVG aspect ratio
    view.set_size_request(1024, 768)
    view.load_uri("http://127.0.0.1:%d/#!snapshot" % port)
    window.add(view)
    window.show_all()
    view.connect("script-alert", on_alert, window)
    gtk.main()

def on_alert(webview, webframe, message, offwindow):
    global info
    cmd = message.split(None, 3)
    if (len(cmd) == 4 and cmd[0] == "snapshot" and
          cmd[1].isdigit() and cmd[2].isdigit()):
        slide, step, xml = int(cmd[1]), int(cmd[2]), cmd[3]
        print "Slide %d step %d" % (slide, step)
        path = "thumbs/%d-%d.png" % (slide, step)

        while len(info) <= slide:
            info.append([])
        info[slide].append(path)

        pixbuf = offwindow.get_pixbuf()
        # XXX Don't hard-code dimensions
        thumb = pixbuf.scale_simple(1024/8, 768/8, gtk.gdk.INTERP_HYPER)
        thumb.save(path, "png")

        file("thumbs-svg/%04d-%04d.svg" % (slide, step), "w").write(xml)
    elif len(cmd) == 1 and cmd[0] == "done":
        json.dump(info, file("thumbs/index.json", "w"))
        sys.exit(0)
    else:
        raise RuntimeError("Unexpected alert %r" % message)
    # Suppress the actual alert dialog
    return True

if __name__ == "__main__":
    main()
