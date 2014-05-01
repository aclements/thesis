// XXX It would be nice if SVG queries were better integrated with
// jQuery.  Actions could then be jQuery methods.  In general, more
// DWIM would make the code terser; for example, passing a
// SVGPathElement to Anim.translate should Just Work.

// XXX The animations created by this are essentially un-debuggable,
// since they're all wrapped up in closure upon closure.  It may be
// nicer to represent them explicitly as objects with the state (and
// perhaps the operation itself) as visible fields.  Then we could
// debug them, optimize them, and possibly manipulate them in more
// powerful ways.

// XXX Make the go bar use captured SVGs rather than requiring a messy
// external thumbnail creation process.  Step through all of the
// animations, capture the SVG XML, probably simplify it (being
// mindful of xlink references), and stick it in the go bar.

// XXX Currently we don't reset changes made to defs.  Copy an
// original copy and use DOM mutation events to determine when it's
// worth resetting?  This might be hugely expensive.

// XXX propRel/propAbs are fine for adjusting .style, but suck at
// adjusting SVG attributes, since they're wrapped up in
// SVGAnimatedNumber and such.

// XXX Replace undiscoverable key bindings with on-screen icons.

var SLIDE_PREVIEWS = false;

//////////////////////////////////////////////////////////////////
// Easing functions
//

/**
 * Penner easing functions.  Each takes a value in the range [0,1] and
 * returns a value in the range [0,1].
 */
Ease = (function() {
    var fns = {};
    function mkEase(name, expr) {
        fns['in' + name] = new Function('t', 'return ' + expr + ';');
        fns['out' + name] = new Function('t', 't=1-t; return 1-(' + expr + ');');
        fns['inOut' + name] = new Function(
            't', ('t*=2; if (t < 1) return (' + expr + ')/2;' +
                  't=2-t; return 1-(' + expr + ')/2;'));
    }
    mkEase('Quad', 't*t');
    mkEase('Cubic', 't*t*t');
    mkEase('Quart', 't*t*t*t');
    mkEase('Quint', 't*t*t*t*t');
    mkEase('Sine', 'Math.cos(t * (Math.PI/2))');
    fns.inOutSine = function(t) { return (Math.cos(Math.PI*t)-1)/-2; };
    mkEase('Expo', 'Math.pow(2, 10 * (t - 1))');
    mkEase('Circ', 'Math.sqrt(1 - t*t) - 1');
    return fns;
})();

//////////////////////////////////////////////////////////////////
// Animation objects
//

/**
 * Create an animation.
 * 
 * new Anim(step, [prepare], [duration]) - Create an animation with a
 * step function.  Prepare is a function to run just prior to the
 * first call to step.  Duration is the duration of the animation in
 * seconds.  If duration is omitted, it defaults to 1.  Functions are
 * called with 'this' bound to the returned animation object.
 * 
 * Step is passed a fractional time on a scale of 0 to 1 (regardless
 * of the animation duration).  For zero duration animations, this
 * will be 1 if the animation is being played forward and 0 if the
 * animation is being played backward.
 * 
 * Prepare will be called when all animations prior to this animation
 * are in their final state and before this animation has first been
 * stepped.  It allows relative animations to capture initial state.
 * It must not modify any document state.
 */
function Anim(step, prepare, duration) {
    this.step = step;
    if (duration === undefined) {
        if (typeof(prepare) === "number") {
            duration = prepare;
            prepare = undefined;
        } else {
            duration = 1;
        }
    }
    this.prepare = prepare === undefined ? nothing : prepare;
    this.duration = duration;
    this._needPrepare = prepare === undefined ? false : true;
    // We cache the last eval result.  This can save a lot of
    // re-computation for heavily shared lower-level animation
    // objects.  [time, value].
    this._cache = [null, null];
}

function nothing() {}

Anim.none = new Anim(nothing, 0);

/**
 * Prepare an animation prior to first playing it by evaluating it at
 * time 1 and then time 0.  This ensures that the initial state of all
 * visuals is as set at the beginning of each animation fragment.  For
 * example, if an animation first reveals an object at time 0.5, this
 * will make sure the object is hidden when the animation starts
 * playing, regardless of the object's base state.
 *
 * XXX This API is easy to use incorrectly by simply forgetting to
 * call it.  Perhaps there should be one type for building up the
 * animation, and freezing it would return a new type that can be
 * played.  Though I use the interchangability right now to deal with
 * steps.
 */
Anim.prototype.stabilize = function() {
    this.eval(1);
    this.eval(0);
};

/**
 * Evaluate this animation at time fraction t.  If the animation has
 * not been preared, it will be prior to calling the step function.
 * If the previous call to eval was for the same t, this will skip the
 * call to step and return a cached value.
 */
Anim.prototype.eval = function(t) {
    if (this._needPrepare) {
        this.prepare();
        this._needPrepare = false;
    }
    var cache = this._cache;
    if (cache[0] !== t) {
        cache[0] = t;
        cache[1] = this.step(t);
    }
    return cache[1];
};

// Compatibility shims
window.requestAnimationFrame =
    window.requestAnimationFrame ||
    window.mozRequestAnimationFrame    ||
    window.webkitRequestAnimationFrame ||
    window.msRequestAnimationFrame     ||
    window.oRequestAnimationFrame;
window.cancelAnimationFrame =
    window.cancelAnimationFrame ||
    window.mozCancelAnimationFrame    ||
    window.webkitCancelAnimationFrame ||
    window.msCancelAnimationFrame     ||
    window.oCancelAnimationFrame;

Anim.prototype._showFrame = function(secs) {
    if (secs > this.duration)
        secs = this.duration;
    try {
        this.eval(this.duration ? secs / this.duration : 1);
    } finally {
        if (secs === this.duration && this.oncomplete)
            this.oncomplete();
    }
    return secs < this.duration;
};

if (window.requestAnimationFrame && window.cancelAnimationFrame) {
    console.log("engine: Using requestAnimationFrame for animation");
    Anim.prototype.start = function() {
        var start = null;
        var anim = this;
        var doFrame = function(now) {
            var abs = 0;
            if (start === null)
                start = now;
            else
                abs = (now - start) / 1000.0;

            if (anim._showFrame(abs))
                anim._frameRequest = window.requestAnimationFrame(doFrame);
            else
                anim._frameRequest = null;
        };

        this.stop();
        this._frameRequest = window.requestAnimationFrame(doFrame);
    };

    Anim.prototype.stop = function() {
        if (this._frameRequest) {
            window.cancelAnimationFrame(this._frameRequest);
            this._frameRequest = null;
        }
    };

    Anim.prototype.isRunning = function() {
        return !!this._frameRequest;
    };
} else {
    console.log("engine: Using setInterval for animation");
    Anim.prototype.start = function() {
        var start = null;
        var anim = this;
        var onInt = function() {
            var abs = 0;
            if (start === null)
                start = new Date().getTime();
            else
                abs = (new Date().getTime() - start) / 1000.0;

            if (!anim._showFrame(abs))
                anim.stop();
        };

        // 30 FPS
        this.stop();
        anim._interval = setInterval(onInt, 33);
        onInt();
    };

    Anim.prototype.stop = function() {
        if (this._interval) {
            clearInterval(this._interval);
            this._interval = null;
        }
    };

    Anim.prototype.isRunning = function() {
        return !!this._interval;
    };
}

//////////////////////////////////////////////////////////////////
// Animation combinators
//

/**
 * Return a new animation whose time scale has been stretched or
 * shrunk to the given absolute time scale.
 */
Anim.prototype.scaleTo = function(duration) {
    var a = this;
    var scaleToStep = function(t) { return a.eval(t); };
    return new Anim(scaleToStep, duration);
};

/**
 * Return an animation that runs twice as fast.
 */
Anim.prototype.faster = function() {
    return this.scaleTo(0.5 * this.duration);
};

/**
 * Return an animation that runs four times faster.
 */
Anim.prototype.fast = function() {
    return this.scaleTo(0.25 * this.duration);
};

/**
 * Return an animation that performs all of the argument animations in
 * sequence.  See Anim.prototype.seq.
 */
Anim.seq = function() {
    var as = arguments;
    if (as.length === 0)
        return Anim.none;
    var anim = as[0];
    for (var i = 1; i < as.length; i++)
        anim = anim.seq(as[i]);
    return anim;
};

/**
 * Return a new animation that first performs this animation, then
 * performs the argument animation.  Prior to preparing the second
 * animation, the first one will be evaluated at time 1.  When an
 * animation is being reversed, the second animation will first be
 * evaluated at time 0, then the first animation will be evaluated.
 */
Anim.prototype.seq = function(b) {
    var a = this;
    var cur = a;
    var duration = a.duration + b.duration;
    // XXX Check for zero duration animations
    var seqStep = function(t) {
        var abs = t * duration;
        if (t !== 1 && (abs < a.duration || t === 0)) {
            if (cur === b)
                b.eval(0);
            cur = a;
            return a.eval(t === 0 ? 0 : abs / a.duration);
        } else {
            if (cur === a)
                a.eval(1);
            cur = b;
            return b.eval(t === 1 ? 1 : (abs - a.duration) / b.duration);
        }
    };        
    return new Anim(seqStep, duration);
};

/**
 * Return an animation that performs all of the argument animations in
 * parallel, all starting at time 0.  The resulting animation's
 * duration is the largest of the argument animations, or 0.
 */
Anim.par = function() {
    var as = arguments;
    var ats = [];
    var duration = 0;
    for (var i = 0; i < as.length; ++i) {
        duration = Math.max(duration, as[i].duration);
        ats[i] = null;
    }
    var parStep = function(t) {
        var abs = t * duration;
        for (var i = 0; i < as.length; ++i) {
            var at;
            if (t === 0 || t === 1)
                at = t;
            else if (as[i].duration === 0)
                at = 1;
            else
                at = Math.min(abs / as[i].duration, 1);
            if (at == ats[i])
                continue;
            as[i].eval(at);
            ats[i] = at;
        }
    };
    return new Anim(parStep, duration);
};

/**
 * Non-static version of Anim.par that combines this animation with
 * one argument animation.
 */
Anim.prototype.par = function(b) {
    return Anim.par(this, b);
};

/**
 * Return a new animation whose time is reversed.  This may not work
 * for highly stateful animators, but can be useful for interpolators
 * and simple actions.
 */
Anim.prototype.reverse = function() {
    var a = this;
    var reverseStep = function(t) { return a.eval(1 - t); };
    return new Anim(reverseStep, a.duration);
};

/**
 * Return a new animation that transforms this animation's time basis
 * using fn.  fn must take a time in [0,1] and return a time in [0,1].
 */
Anim.prototype.easeBy = function(fn) {
    var a = this;
    function easeStep(t) {
        return a.eval(fn(t));
    };
    return new Anim(easeStep, a.duration);
};

$.each(Ease, function(name, fn) {
    Anim.prototype['ease' + name.charAt(0).toUpperCase() + name.slice(1)] =
        function() { return this.easeBy(fn); };
});

/**
 * Return a new animation that will first call the given prepare
 * function and then proceed with this animation.
 */
Anim.prototype.withPrepare = function(prepare) {
    return new Anim(undefined, prepare, 0).seq(this);
};

Anim.prototype.svgSuspend = function(svg) {
    var a = this;
    var svgsvg = svg.rootElement;
    var svgSuspendStep = function(t) {
        var susp = svgsvg.suspendRedraw(100);
        var res = a.eval(t);
        svgsvg.unsuspendRedraw(susp);
        return res;
    };
    return new Anim(svgSuspendStep, a.duration);
};

//////////////////////////////////////////////////////////////////
// Animation adaptors
//

/**
 * A list of functions to call after setup to undo hacks.  For some
 * hacks, performance is abysmal if we have to do them and undo them
 * repeatedly during setup, so it's useful to defer to undo step.  If
 * this is null, setup hacks will be undone immediately.  If this is a
 * list, undo steps will be appended and, after setup is done, they
 * will be executed in reverse order.
 */
var setupHackUndo = null;

/**
 * Convert a point in obj's coordinate space to the user coordinate
 * space of obj's owning SVG element.  Returns an array of [x,y].
 */
function pointToUCS(obj, x, y) {
    var point = obj.ownerSVGElement.createSVGPoint();
    point.x = x;
    point.y = y;
    // We *don't* want the CTM because that includes the viewbox
    // transform of the viewport.  Instead we want the transform only
    // as far as the *user* coordinate space of the owning element so
    // that our points are not affected by viewbox changes.  We call
    // this the UTM: user transform matrix.
    var utm = obj.getTransformToElement(obj.ownerSVGElement);
    var npoint = point.matrixTransform(utm);
    return [npoint.x, npoint.y];
}

/**
 * Convert a point from the user coordinate space of obj's owning SVG
 * element into obj's coordinate space.  Returns an array of [x,y].
 */
function pointFromUCS(obj, x, y) {
    var point = obj.ownerSVGElement.createSVGPoint();
    point.x = x;
    point.y = y;
    var utm = obj.getTransformToElement(obj.ownerSVGElement);
    var ipoint = point.matrixTransform(utm.inverse());
    return [ipoint.x, ipoint.y];
}

/**
 * Construct a read-only handle at the fixed location <x, y>.
 */
function FixedHandle(x, y) {
    this._pos = [x, y];
}

FixedHandle.prototype = {
    get pos() {
        return this._pos;
    },
    detach: function() {
        return this;
    },
};

/**
 * Construct a handle to the center of the bounding box of an SVG
 * object.  The position of this handle can be retrieved and moved in
 * the user coordinate space of the containing SVG.
 *
 * obj must implement SVGLocatable to get the .pos field.  obj must
 * further implement SVGTransformable to set the .pos field.
 */
function BBoxHandle(obj) {
    this._obj = obj;
}

BBoxHandle.prototype = {
    _getBBox: function() {
        var obj = this._obj;
        try {
            var bbox = obj.getBBox();
            // Chrome sometimes returns an empty bounding box for
            // invisible elements, so check for this.
            if (bbox.width !== 0 && bbox.height !== 0)
                return bbox;
        } catch (err) {
            // Firefox can't get bounding boxes of display:none
            // elements.  Work around this by temporarily showing
            // them.  Definitely not fast, but at least it works.
            // Since we set up all animation during start-up, the
            // slowness of this is generally okay.
        }
        // Make element visible
        var fixed = [];
        for (var p = obj; p; p = p.parentNode) {
            if (p.style && p.style.display === "none") {
                fixed.push(p);
                p.style.display = "inline";
            }
        }
        var bbox = obj.getBBox();
        // Undo hack
        var undo = function() {
            for (var i = 0; i < fixed.length; i++)
                fixed[i].style.display = "none";
        };
        if (setupHackUndo === null)
            undo();
        else
            setupHackUndo.push(undo);
        return bbox;
    },
    get pos() {
        var bbox = this._getBBox();
        return pointToUCS(this._obj,
                          bbox.x + bbox.width/2, bbox.y + bbox.height/2);
    },
    set pos(val) {
        var obj = this._obj;
        // XXX Would be more stable to reuse the last matrix, if we made it
        obj.transform.baseVal.consolidate();

        var ipoint = pointFromUCS(obj, val[0], val[1]);
        var bbox = this._getBBox();
        var xlate = obj.ownerSVGElement.createSVGTransform();
        xlate.setTranslate(ipoint[0] - (bbox.x + bbox.width/2),
                           ipoint[1] - (bbox.y + bbox.height/2));
        obj.transform.baseVal.appendItem(xlate);
    },
    detach: function() {
        var xy = this.pos;
        return new FixedHandle(xy[0], xy[1]);
    },
};

/**
 * Construct a handle to the n'th point of the given SVGPathElement.
 * The position of this handle can be retrieved and moved in the user
 * coordinate space of the containing SVG.
 */
function PathHandle(path, n) {
    function normalizePath(path) {
        var sl = path.pathSegList;
        var x = 0, y = 0;
        for (var i = 0; i < sl.numberOfItems; i++) {
            var seg = sl.getItem(i);
            var type = seg.pathSegTypeAsLetter;
            if (/[ML]/.test(type)) {
                x = seg.x;
                y = seg.y;
            } else if (/[ml]/.test(type)) {
                x += seg.x;
                y += seg.y;
                switch (type) {
                case 'm':
                    sl.replaceItem(path.createSVGPathSegMovetoAbs(x, y), i);
                    break;
                case 'l':
                    sl.replaceItem(path.createSVGPathSegLinetoAbs(x, y), i);
                    break;
                }
            } else {
                throw "Unsupported path segment operator " + type;
            }
        }
    };

    normalizePath(path);
    this._path = path;
    this._seg = path.pathSegList.getItem(n);
}

PathHandle.prototype = {
    get pos() {
        // XXX Use a cache?
        return pointToUCS(this._path, this._seg.x, this._seg.y);
    },
    set pos(val) {
        var ipoint = pointFromUCS(this._path, x, y);
        this._seg.x = ipoint[0];
        this._seg.y = ipoint[1];
    },
    detach: function() {
        var xy = this.pos;
        return new FixedHandle(xy[0], xy[1]);
    },
};

/**
 * DWIM constructor for Handles.  If obj is already a BBoxHandle or a
 * PathHandle, simply returns obj.  Otherwise, this constructs a new
 * BBoxHandle from obj.
 */
function autoHandle(obj) {
    if (obj instanceof FixedHandle || obj instanceof BBoxHandle ||
        obj instanceof PathHandle)
        return obj;
    return new BBoxHandle(obj);
}

//////////////////////////////////////////////////////////////////
// Interpolators
//

var Inter = {};

/**
 * Interpolate using a JavaScript expression of 't'.
 */
Inter.expr = function(expr) {
    var exprStep = new Function('t', 'return ' + expr);
    return new Anim(exprStep);
};

/**
 * Interpolate between the numbers start and end.  The returned Anim
 * will have fields 'start' and 'end' that can be dynamically updated.
 */
Inter.number = function(start, end) {
    var numberStep = function (t) {
        return (1-t) * this.start + t * this.end;
    };
    var a = new Anim(numberStep);
    a.start = start;
    a.end = end;
    return a;
};

function parseColor(c) {
    var m = c.match(/^#([0-9a-f]{6})$/i);
    if (m) {
        c = parseInt(c.substr(1), 16);
        return [(c>>16)&0xFF, (c>>8)&0xFF, c&0xFF];
    }
    m = c.match(/^rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$/i);
    if (m)
        return [parseInt(m[1]), parseInt(m[2]), parseInt(m[3])];
    throw "Can't parse color " + c;
}

function mkColor(r, g, b) {
    r = Math.round(r); g = Math.round(g); b = Math.round(b);
    return "rgb(" + r + "," + g + "," + b + ")";
}

Inter.color = function(start, end) {
    start = parseColor(start);
    end = parseColor(end);
    var colorStep = function (t) {
        var tp = 1-t;
        return mkColor(start[0] * tp + end[0] * t,
                       start[1] * tp + end[1] * t,
                       start[2] * tp + end[2] * t);
    };
    return new Anim(colorStep);
};

/**
 * Interpolate a point between [0,0] and [x,y] or, if x2 and y2 are
 * provided, between [x,y] and [x2,y2].
 */
Inter.seg = function(x, y, x2, y2) {
    if (x2 === undefined && y2 === undefined) {
        var vectorStep = function (t) {
            return [x*t, y*t];
        };
    } else {
        var xd = x2 - x, yd = y2 - y;
        var vectorStep = function (t) {
            return [x + xd*t, y + yd*t];
        };
    }
    var a = new Anim(vectorStep);
    return a;
};

/**
 * Interpolate a point between two autoHandle-able objects.  The
 * returned Anim will have fields 'start' and 'end' storing the start
 * and end handles.  Note that this interpolation is done dynamically,
 * so if start or end are being moved, the interpolation will reflect
 * that.
 */
Inter.segBetween = function(start, end) {
    var segBetweenStep = function (t) {
        var a = this.start.pos, b = this.end.pos;
        return [a[0] * (1 - t) + b[0] * t,
                a[1] * (1 - t) + b[1] * t];
    };
    var a = new Anim(segBetweenStep);
    a.start = autoHandle(start);
    a.end = autoHandle(end);
    return a;
};

Inter.path = function(path, start, end) {
    if (start === undefined)
        start = 0;
    if (end === undefined)
        end = 1;
    var pathStep = function(t) {
        var len = path.getTotalLength();
        var base = path.getPointAtLength(start * len);
        var baseUCS = pointToUCS(path, base.x, base.y);
        var pos = path.getPointAtLength(((1 - t) * start + t * end) * len);
        var posUCS = pointToUCS(path, pos.x, pos.y);
        return [posUCS[0] - baseUCS[0], posUCS[1] - baseUCS[1]];
    };
    return new Anim(pathStep);
};

//////////////////////////////////////////////////////////////////
// Actions
//

var Action = {};

Action._array = function(maybeArray) {
    if (maybeArray.length === undefined)
        return [maybeArray];
    if (maybeArray.length === 0)
        throw "Action applied to empty array: " + maybeArray.query
    return maybeArray;
};

Action.propAbs = function(objs, prop, timedVal) {
    objs = Action._array(objs);
    var propAbsStep = function (t) {
        var val = timedVal.eval(t);
        for (var i = 0; i < objs.length; ++i)
            objs[i][prop] = val;
    };
    return new Anim(propAbsStep);
};

Action.propRel = function(objs, prop, timedVal) {
    objs = Action._array(objs);
    var init = [];
    var propRelInit = function () {
        for (var i = 0; i < objs.length; ++i)
            init[i] = objs[i][prop];
    };
    var propRelStep = function (t) {
        var val = timedVal.eval(t);
        for (var i = 0; i < objs.length; ++i)
            objs[i][prop] = init[i] + val;
    };
    return new Anim(propRelStep, propRelInit);
};

/**
 * Return an action that translates points by a vector, where a vector
 * of 0,0 is the current position of points.
 */
Action.translateRel = function(points, timedVector) {
    points = Action._array(points);
    var init = [];
    for (var i = 0; i < points.length; ++i)
        points[i] = autoHandle(points[i]);
    var translateInit = function() {
        for (var i = 0; i < points.length; ++i) {
            init[i] = points[i].pos;
        }
    };
    var translateStep = function(t) {
        var off = timedVector.eval(t);
        for (var i = 0; i < points.length; ++i)
            points[i].pos = [init[i][0] + off[0], init[i][1] + off[1]];
    };
    return new Anim(translateStep, translateInit);
};

Action.translateAbs = function(points, timedVector) {
    points = Action._array(points);
    for (var i = 0; i < points.length; ++i)
        points[i] = autoHandle(points[i]);
    var translateAbsStep = function(t) {
        var pos = timedVector.eval(t);
        for (var i = 0; i < points.length; ++i)
            points[i].pos = pos;
    };
    return new Anim(translateAbsStep);
};

Action.rotate = function(objs, timedAngle, center) {
    objs = Action._array(objs);
    center = autoHandle(center);
    var svg = objs[0].ownerSVGElement;
    var xforms = [];
    var rotateInit = function() {
        for (var i = 0; i < objs.length; i++) {
            xforms[i] = svg.createSVGTransform();
            objs[i].transform.baseVal.appendItem(xforms[i]);
        }
    };
    var rotateStep = function(t) {
        var angle = timedAngle.eval(t);
        var c = center.pos;
        var point = svg.createSVGPoint();
        point.x = c[0];
        point.y = c[1];
        for (var i = 0; i < objs.length; i++) {
            // c is in the user coordinate space.  Convert it into
            // each object's coordinate space.
            // XXX This would be easier with our own point abstraction
            // that knew what coordinate space it was in.
            var utm = svg.getTransformToElement(objs[i]);
            var oc = point.matrixTransform(utm);
            xforms[i].setRotate(angle, oc.x, oc.y);
        }
    };
    return new Anim(rotateStep, rotateInit);
};

Action.reveal = function(objs) {
    objs = Action._array(objs);
    var revealStep = function (t) {
        for (var i = 0; i < objs.length; ++i) {
            var obj = objs[i];
            // Workaround: Chrome crashes if you change a tspan from
            // display:none to display:inline
            if (obj.nodeName === "tspan") {
                obj.style.display = "";
                if (t === 0)
                    obj.style.opacity = 0;
                else
                    obj.style.opacity = 1;
            } else {
                if (t === 0)
                    obj.style.display = "none";
                else
                    obj.style.display = "inline";
            }
        }
    };
    return new Anim(revealStep, 0);
};

Action.unreveal = function(objs) {
    return Action.reveal(objs).reverse();
};

Action.fade = function(objs, start, end) {
    objs = Action._array(objs);
    var styles = [];
    for (var i = 0; i < objs.length; ++i) {
        // Workaround: Chrome ignores opacity on text elements with
        // display:inline.
        if (objs[i].nodeName === "text" && objs[i].style.display === "inline")
            objs[i].style.display = "";
        styles[i] = objs[i].style;
    }

    if (start === undefined)
        start = 0;
    // XXX Capture end from object?
    if (end === undefined)
        end = 1;
    return Action.propAbs(styles, "opacity", Inter.number(start, end));
};

Action.fadeOut = function(objs) {
    return Action.fade(objs, 1, 0);
};

Action.fadeTo = function(objs, end) {
    objs = Action._array(objs);

    // Workaround
    for (var i = 0; i < objs.length; ++i)
        if (objs[i].nodeName === "text" && objs[i].style.display === "inline")
            objs[i].style.display = "";

    var inter = [];
    var fadeToInit = function () {
        for (var i = 0; i < objs.length; ++i) {
            if (objs[i].style.opacity === "")
                objs[i].style.opacity = 1;
            inter[i] = Inter.number(objs[i].style.opacity, end);
        }
    };
    var fadeToStep = function (t) {
        for (var i = 0; i < objs.length; ++i)
            objs[i].style.opacity = inter[i].eval(t);
    };
    return new Anim(fadeToStep, fadeToInit);
};

Action.dim = function(objs) {
    return Action.fadeTo(objs, 0.5);
};

Action.undim = function(objs) {
    return Action.fadeTo(objs, 1);
};

/**
 * Work around redraw bugs in Firefox by forcing a redraw of the
 * entire SVG.
 */
Action.forceRedraw = function(objs) {
    var root = objs[0].ownerDocument.rootElement;
    var forceRedrawStep = function() {
        // You'd think we could just use forceRedraw, but that seems
        // to have the very mis-optimizations we're working around.
        // However, futzing with the style of the SVG element works.
        root.style.color = root.style.color;
    };
    return new Anim(forceRedrawStep, 0);
};


//////////////////////////////////////////////////////////////////
// SVG/jQuery helpers
//

var inkscapeNS = "http://www.inkscape.org/namespaces/inkscape";

jQuery.expr[':'].inklayer = function (elem) {
    return (elem.nodeName === "g" &&
            elem.getAttributeNS(inkscapeNS, "groupmode") === "layer");
};

jQuery.fn.inklabel = function () {
    if (this.length === 0)
        throw "No elements";
    return this[0].getAttributeNS(inkscapeNS, "label");
};

(function() {
    var labelRe = /^\?(.+)/, idRe = /^#(.+)/;

    var SVGWrapper = function(doc, query) {
        this._doc = doc;
        this._labelMap = null;
        this.query = query;
        var othis = this;
        this.finder = function(q) { return othis.find(q); };
    };

    // Make SVGWrapper array-like
    SVGWrapper.prototype.length = 0;
    SVGWrapper.prototype.splice = Array.prototype.splice;
    SVGWrapper.prototype.push = Array.prototype.push;

    SVGWrapper.prototype.find = function(q) {
        var res = new SVGWrapper(this._doc, q);
        var parts = q.split(",");
        for (var i = 0; i < parts.length; i++)
            this._find1(res, parts[i]);
        return res;
    };

    SVGWrapper.prototype._getLabelMap = function() {
        if (this._labelMap === null) {
            // Construct the labelMap
            var labelMap = {};
            for (var ei = 0; ei < this.length; ++ei) {
                var elts = this[ei].getElementsByTagName("*");
                for (var i = 0; i < elts.length; i++) {
                    var labelStr = $(elts[i]).inklabel();
                    if (!labelStr)
                        continue;
                    var labels = labelStr.split(/\s+/);
                    for (var li = 0; li < labels.length; li++) {
                        var label = labels[li];
                        var have = labelMap[label];
                        if (have)
                            have.push(elts[i]);
                        else
                            labelMap[label] = [elts[i]];
                    }
                }
            }
            this._labelMap = labelMap;
        }
        return this._labelMap;
    };

    SVGWrapper.prototype._find1 = function(res, q) {
        q = q.trim();
        var m;
        if ((m = labelRe.exec(q))) {
            var labelMap = this._getLabelMap();
            var lm = labelMap[m[1]];
            if (lm) {
                for (var i = 0; i < lm.length; ++i) {
                    res.push(lm[i]);
                }
            }
        } else if ((m = idRe.exec(q))) {
            var elt = this._doc.getElementById(m[1]);
            if (elt !== null)
                res.push(elt);
        } else {
            throw "Bad svgWrap query " + q;
        }
    };

    SVGWrapper.prototype.maybeOne = function() {
        if (this.length === 1) {
            return this[0];
        } else if (this.length === 0) {
            return null;
        } else {
            throw "Multiple elements";
        }
    };

    SVGWrapper.prototype.one = function() {
        if (this.length === 1) {
            return this[0];
        } else if (this.length === 0) {
            throw "No elements";
        } else {
            throw "Multiple elements";
        }
    };

    var svgWrap = function(svg) {
        if (svg.getSVGDocument !== undefined)
            svg = svg.getSVGDocument();
        var doc = svg.ownerDocument;

        var res = new SVGWrapper(doc);
        res.push(svg);
        return res;
    };

    window.svgWrap = svgWrap;
})();

//////////////////////////////////////////////////////////////////
// Slide display and management
//

/**
 * Construct a slide deck inside the given DOM parent elements from a
 * path to an SVG document and a dictionary of transition generators.
 *
 * In the SVG, top-level layers become slides, unless their label
 * begins with a colon, in which case they are hidden.  The labels of
 * these top-level layers are used as their titles, and appear both in
 * the master slide and are used as an index into the transitions
 * dictionary.  A slide may also have sublayers, in which case each
 * will be revealed in a separate step of that slide.
 *
 * Layers labeled ":master" become the master slide for all slides
 * following them, up to the next ":master" slide (if any).  If the
 * master slide has a text object labeled "title", its contents will
 * be substituted with the current slide's title.
 *
 * The transitions dictionary maps from "<title>" (for slides without
 * sub-layers) or "<title>/<subtitle>" (for slides with sub-layers) to
 * transition generators.  A transition generator is a function that
 * returns an animation, or a (potentially nested) array of
 * animations.  Each of these animations will be used as a separate
 * step for that slide or sub-slide.  The transition generator is
 * called with "this" bound to the SVG wrapper of the SVG element
 * representing the slide.  If the transition generator is for a
 * sublayer, the transition generator will be passed the SVG wrapper
 * for the sublayer's SVG element as an argument.
 *
 * The transitions dictionary can also contain inter-slide transitions
 * named "<title1>+<title2>".  These functions will be passed SVG
 * wrappers for slide title1 and slide title2 and must return a single
 * animation.
 */
function Slides(parent, path, transitions, external) {
    // Create slides DOM
    var loading = $("<div>Loading</div>");
    loading.css("position", "relative").width("100%").height("0px").
        css("text-align", "center");
    $(parent).append(loading);

    var svgObj = $('<object type="image/svg+xml"></object>');
    // We set visibility instead of display because some browsers
    // won't even load the object if display is "none".
    svgObj.css("visibility", "hidden").width("100%").height("100%");
    svgObj.attr("data", "slides.svg");
    $(parent).append(svgObj);

    this._preShowHook = [];
    this._cursorHook = [];
    this._fullScreenHook = [];

    function arrayEq(a, b) {
        if (a.length !== b.length)
            return false;
        for (var i = 0; i < a.length; i++)
            if (a[i] !== b[i])
                return false;
        return true;
    }

    var slidesthis = this;
    var onload = function() {
        var svg = svgObj[0].getSVGDocument();

        // Workaround a bug in Chrome (seen in 27.0.1453.15) where
        // pages take a very long time to *unload* when they contain a
        // large SVG.  (Even when opening the SVG directly, navigating
        // away from it can take several seconds for a 500K SVG.)
        $(window).bind('beforeunload', function() {
            // Remove the children in *reverse* order.  Removing them
            // the obvious way or just removing the 'svg' directly
            // takes forever.  My guess is that what takes so long is
            // removing the targets of xlinks in the defs section.
            // xlinks don't have to go up-document, but the majority
            // of them do, so this works well.
            var children = $("svg > *", svg);
            for (var i = children.length - 0; i >= 0; i--)
                $(children[i]).remove();
        });

        // Workaround Chrome not honoring xml:space="preserve" by
        // replacing it with "white-space: pre"
        $("[xml\\:space=preserve] tspan", svg).css("white-space", "pre");

        // Don't display selection in the SVG.  Otherwise, clicking
        // rapidly through slides tends to select text.
        $(svg.documentElement).css(
            {"-webkit-touch-callout": "none",
             "-webkit-user-select": "none",
             "-khtml-user-select": "none",
             "-moz-user-select": "none",
             "-ms-user-select": "none",
             "user-select": "none"});

        // Hide objects labeled "anim" (these are used for animation
        // paths and such) and symbols
        $.each(svgWrap(svg).find("?anim, ?symbols"), function () {
            this.style.display = "none";
        });

        // Find notes and add a "notes" CSS class so we can easily
        // control their visibility
        function checkFill(obj, fill) {
            if (!obj.style || !obj.style.fill)
                return false;
            try {
                var rgb = parseColor(obj.style.fill);
            } catch (e) {
                return false;
            }
            return arrayEq(rgb, fill);
        }
        $.each($("text", svg), function() {
            if (checkFill(this, [255, 0, 0])) {
                this.classList.add("notes");
                this.style.display = "";
            }
        });
        $.each($("rect", svg), function() {
            if (checkFill(this, [255, 227, 227])) {
                this.classList.add("notes");
                this.style.display = "";
            }
        });

        // Add the style block for controlling default note visibility
        this._noteStyle = $('<style>').appendTo(svg.documentElement);
        this.showNotes(external);

        // Construct slides
        var slides = [];
        var cur = {slide: null, master: null};

        // Find master slides
        var mastersUnder = [], mastersOver = [];
        $("svg > g", svg).each(function (i) {
            var name = $(this).inklabel();
            if (name === ":master-under" || name === ":master")
                mastersUnder[i] = this;
            else if (name === ":master-over")
                mastersOver[i] = this;
            else
                mastersUnder[i] = mastersOver[i] = null;
        });
        for (var i = 1; i < mastersUnder.length; i++)
            mastersUnder[i] = mastersUnder[i] || mastersUnder[i - 1];
        for (var i = mastersOver.length - 2; i >= 0; i--)
            mastersOver[i] = mastersOver[i] || mastersOver[i + 1];

        var masters = [];
        for (var i = 0; i < mastersUnder.length; i++) {
            if (i > 0 && mastersUnder[i] === mastersUnder[i - 1] &&
                mastersOver[i] === mastersOver[i - 1])
                masters[i] = masters[i - 1];
            else
                masters[i] = new Master(mastersUnder[i], mastersOver[i], cur);
        }

        // Turn top-level groups into slides and hide everything
        $("svg > g", svg).each(function (i) {
            this.style.display = "none";

            var name = $(this).inklabel();
            if (name === ":crop") {
                // XXX This is a hack to make it easy to draw black
                // boxes over the edges of the page in order to "crop"
                // any overflow away.  It would be much better to do
                // this automatically.
                this.style.display = "inline";
                return;
            } else if (name[0] === ":") {
                return;
            }

            var prevSlide = slides.length ? slides[slides.length - 1] : null;
            var slide = new Slide(slides.length, this, masters[i], transitions,
                                  cur, othis, prevSlide);
            slides.push(slide);
        });

        this._svg = svg;
        this._slides = slides;
        this._cur = cur;

        loading.hide();
        svgObj.css("visibility", "visible");
        this.show(0);

        // XXX Snapshot mode
        this._snapshotMode = false;
        if (location.hash === "#!snapshot") {
            this._snapshotMode = true;
            location.hash = "";
            this.doSnapshot();
        }

        // Create the go bar
        if (!external && !this._snapshotMode)
            this._makeGoBar(parent);

        if (this.onload)
            this.onload();
    };
    var othis = this;
    svgObj[0].onload = function() {
        // Workaround: If onload throws an exception, Chrome will use
        // the cached SVG until you close the whole window, so leave
        // onload before setting up.  The same thing happens with a
        // really short timeout; 10 seems to work.
        setTimeout(jQuery.proxy(onload, othis), 10);
    };
}

/**
 * Bind key and mouse events in document to cause transitions in this
 * slide deck.
 */
Slides.prototype.bindEvents = function() {
    if (!this._svg)
        throw "Slides SVG not loaded yet";
    if (this._snapshotMode)
        return;

    var othis = this;
    var K_SPACE = 32, K_PAGE_UP = 33, K_PAGE_DOWN = 34;
    var K_END = 35, K_HOME = 36;
    var K_B = 66, K_F = 70, K_N = 78, K_X = 88;
    var keydown = function(event) {
        if (event.which == K_PAGE_UP) {
            if (event.shiftKey)
                othis.show(othis._cur.slide.n - 1);
            else
                othis.prev();
        } else if (event.which == K_SPACE || event.which == K_PAGE_DOWN) {
            if (event.shiftKey)
                othis.show(othis._cur.slide.n + 1);
            else
                othis.next();
        } else if (event.which == K_END) {
            if (event.shiftKey)
                // XXX Push a breadcrumb
                othis.show(othis._slides.length - 1);
            else
                othis.show(othis._cur.slide.n, "last", true);
        } else if (event.which == K_HOME) {
            if (event.shiftKey)
                // XXX Push a breadcrumb
                othis.show(0);
            else
                othis.show(othis._cur.slide.n);
        } else if (event.which == K_X) {
            othis.openExternal();
        } else if (event.which == K_B) {
            for (var i = 0; i < othis._fullScreenHook.length; i++)
                othis._fullScreenHook[i]();
        } else if (event.which == K_F) {
            // XXX Support other browsers
            document.documentElement.webkitRequestFullScreen();
        } else if (event.which == K_N) {
            othis.showNotes(!othis._notesVisible);
        }
    };

    // Set up slide transitions
    $(document).click($.proxy(this.next, this));
    $(document).keydown(keydown);
    $(this._svg).click($.proxy(this.next, this));
    $(this._svg).keydown(keydown);

    // Set up cursor hiding
    var timeout = null;
    var hideCursor = function () {
        othis._svg.documentElement.style.cursor = "none";
        for (var i = 0; i < othis._cursorHook.length; i++)
            othis._cursorHook[i](false);
    };
    var last = [-1,-1];
    this._svg.onmousemove = function (event) {
        // On Chrome, at least, hiding the cursor generates a motion
        // event, so check if it really moved.
        if (last[0] == event.clientX && last[1] == event.clientY)
            return;
        last[0] = event.clientX;
        last[1] = event.clientY;

        // Show the cursor
        othis._svg.documentElement.style.cursor = "";

        // Prepare to hide the cursor again
        if (timeout)
            window.clearTimeout(timeout);
        timeout = window.setTimeout(hideCursor, 1000);

        for (var i = 0; i < othis._cursorHook.length; i++)
            othis._cursorHook[i](true);
    };
    hideCursor();

    // Handle the document hash
    if (location.hash)
        this.show(location.hash.replace(/^#/, ""));
    this._updateHash = true;
};

Slides.prototype.showNotes = function(show) {
    if (show)
        this._noteStyle.text('.notes {display:inline;}');
    else
        this._noteStyle.text('.notes {display:none;}');
    this._notesVisible = show;
};

Slides.prototype.doSnapshot = function() {
    // Force a refresh before snapshotting (10ms isn't enough, 100ms
    // seems to be)
    var othis = this;
    setTimeout(function() {
        var cur = othis._cur.slide;
        var xml = othis._svg.documentElement.cloneNode(true);
        var xmls = new XMLSerializer().serializeToString(xml);
        alert("snapshot " + cur.n + " " + cur.step + " " + xmls);
        if (cur.step < cur.nsteps - 1)
            othis.show(cur.n, cur.step + 1, true);
        else if (cur.n < othis._slides.length - 1)
            othis.show(cur.n + 1, 0, true);
        else {
            alert("done");
            return;
        }
        othis.doSnapshot();
    }, 100);
};

// The rules for slide actions are:
// * Next at the end of a step: begin next step
// * Next in the middle of a step: jump to end of step
// * Previous at the end of a step: jump to end of previous step
// * Previous in the middle of a step: jump to end of previous step
// * Jump to slide n: begin first step
// * Jump to slide n, step m: jump to end of step m

/**
 * Show a particular slide number, optionally at some step, and
 * optionally at the end of that step.  If step and end are not
 * specified, show the beginning of the slide and start animating the
 * first step.  If step is specified, jump straight to that step and,
 * if end is true, jump to the end of the step (so no animation takes
 * place).
 */
Slides.prototype.show = function(n, step, end) {
    if (!this._svg)
        throw "Slides SVG not loaded yet";

    if (typeof n === "string") {
        // Find this slide by id (we could use the title, but id's are
        // generally stable)
        for (var i = 0; i < this._slides.length; i++) {
            if (this._slides[i].elt.id === n) {
                n = i;
                break;
            }
        }
        if (n !== i)
            // Oh well
            n = 0;
    }

    if (n < 0 || n >= this._slides.length)
        return;

    if (step === undefined)
        step = 0;
    else if (step === "last")
        step = this._slides[n].nsteps - 1;

    this._slides[n].show(step, end);

    if (this._updateHash)
        location.hash = this._slides[n].elt.id;

    for (var i = 0; i < this._preShowHook.length; i++) {
        // XXX If we open and close the external display, this will
        // have stale hooks that have a nasty habit of throwing
        // exceptions.  We should somehow clean these up, but for now
        // we'll just trap their exceptions and move on.
        try {
            this._preShowHook[i](n, step, end);
        } catch (err) {
            console.error("preShowHook exception: " + err);
        }
    }
};

Slides.prototype.next = function() {
    var cur = this._cur.slide;

    // If we're running an animation, finish it
    if (this._cur.anim && this._cur.anim.isRunning()) {
        this.show(cur.n, cur.step, true);
        return;
    }

    // Move to the next step if possible
    if (cur.step < cur.nsteps - 1) {
        this.show(cur.n, cur.step + 1);
        return;
    }

    // Move to the next slide if possible
    this.show(cur.n + 1);
};

Slides.prototype.prev = function() {
    // Move to the previous step if possible
    var cur = this._cur.slide;
    if (cur.step > 0) {
        this.show(cur.n, cur.step - 1, true);
        return;
    }

    // Move to the previous slide if possible
    if (cur.n > 0)
        this.show(cur.n - 1, "last", true);
};

Slides.prototype.openExternal = function() {
    // XXX This is a mess.  User actions should be handled by a
    // controller, which should broadcast an event (possibly
    // determined by the state of the main Slides object) and various
    // listeners should update their local state.  Like Qt's signals
    // and slots.  The two display types should be more symmetric;
    // e.g., so when working on slides you can flip between projected
    // view and presenter view in one window and be able to navigate
    // in presenter view.
    var ext = window.open("external.html", "external", 
                          "location=no,status=no,toolbar=no");
    var othis = this;
    var link = function(other, next) {
        var oshow = function(n, step, end) {
            if (!other._svg)
                return;
            if (next)
                step++;
            if (step < other._slides[n].nsteps)
                other.show(n, step, true);
            else
                other.show(n + 1, 0, true);
        };
        othis._preShowHook.push(oshow);
        other.onload = function() {
            oshow(othis._cur.slide.n, othis._cur.slide.step, true);
        };
    };
    ext.onload = function() {
        // XXX Hard-coded badness
        // XXX Would probably be much less resource-intensive if we
        // just swapped cur and next and updated only one of them
        var cur = new Slides($("#cur", ext.document), "slides.svg", transitions, true);
        link(cur, false);
        var next = new Slides($("#next", ext.document), "slides.svg", transitions, true);
        link(next, true);
        var notes = $("#notes", ext.document);
        var notesData = null;

        var loadNotes = function () {
            if (notesData === null)
                return;
            var slideName = othis._cur.slide.elt.id;
            for (var i = 0; i < notesData.length; i++) {
                if (notesData[i].name === slideName) {
                    notes.val(notesData[i].notes);
                    return;
                }
            }
            notes.val("");
        };

        // Get notes data
        notes.val("Loading...");
        notes.attr("readonly", "readonly");
        jQuery.getJSON(
            "notes.json?" + Date.now(),
            function (data) {
                notesData = data;
                loadNotes();
                // Try posting the notes back
                jQuery.ajax({
                    type: "POST",
                    url: "post-notes",
                    data: JSON.stringify(notesData),
                    success: function () {
                        notes.attr("readonly", "");
                    }
                });
            });

        // Update notes when changing slides
        othis._preShowHook.push(loadNotes);

        // Save notes on change
        var saveTimeout = null, saving = false;
        var saveNotes = function () {
            if (saving) {
                // We're still waiting on a POST
                saveTimeout = setTimeout(saveNotes, 5000);
                // Don't let the user get too far ahead
                notes.attr("readonly", "readonly");
                return;
            }

            saveTimeout = null;
            saving = true;
            jQuery.ajax({
                type: "POST",
                url: "post-notes",
                data: JSON.stringify(notesData),
                error: function (xhr, textStatus, errorThrown) {
                    alert("Failed to save notes: " + textStatus + ", " + errorThrown);
                },
                complete: function () {
                    saving = false;
                    notes.attr("readonly", "");
                }
            });
        };
        notes.keyup(function () {
            // XXX This gets called other random times (when losing
            // focus?) which is unfortunate when the notes server
            // isn't available.  Also, it doesn't get called at useful
            // times (like after a paste).

            // Create id-to-notes map
            var notesIndex = {};
            for (var i = 0; i < notesData.length; i++)
                notesIndex[notesData[i].name] = notesData[i];

            // Create new notes array
            var notesNew = [];
            for (var i = 0; i < othis._slides.length; i++) {
                var slide = othis._slides[i];
                if (slide === othis._cur.slide) {
                    if (notes.val() !== "")
                        notesNew.push({"name" : slide.elt.id,
                                       "title" : slide.title,
                                       "notes" : notes.val()});
                } else if (slide.elt.id in notesIndex) {
                    notesNew.push(notesIndex[slide.elt.id]);
                }
                delete notesIndex[slide.elt.id];
            }

            // Append any dangling notes (preserving order)
            for (var i = 0; i < notesData.length; i++) {
                if (notesData[i].name in notesIndex)
                    notesNew.push(notesData[i]);
            }

            // Done
            if (notesData == notesNew)
                return;
            notesData = notesNew;

            // Post new notes
            if (saveTimeout === null)
                saveTimeout = setTimeout(saveNotes, 5000);
        });

        // Auto-scroll notes on step change
        var scrollNotes = function(n, step, end) {
            var pct = step / (othis._slides[n].nsteps - 1);
            // scrollHeight isn't W3C, but is supported in Firefox 3+
            // and Chrome 4+.  scrollHeight is the entire scrollable
            // height, so we subtract out the visible height.
            var max = notes[0].scrollHeight - notes.innerHeight();
            notes.animate({scrollTop: pct*max});
        };
        othis._preShowHook.push(scrollNotes);

        // Full-screen current slide preview
        var isFull = false;
        var fullScreen = function() {
            isFull = !isFull;
            var val = isFull ? "100%" : "";
            $("#cur", ext.document).css("width", val).css("height", val);
        };
        othis._fullScreenHook.push(fullScreen);

        // Show working bar when animating
        var working = $("#working", ext.document);
        othis.preAnimHook = function() {
            working.css("visibility", "visible");
        };
        othis.postAnimHook = function() {
            working.css("visibility", "hidden");
        };
    };
};

Slides.prototype._makeGoBar = function(parent) {
    var slides = this;
    var previews = {};
    // XXX There should be *one* of these on the external display,
    // which suggests this should be a function of the a controller,
    // not Slides.

    // Create the basic DOM
    var goDiv = $('<div class="slides-go"></div>').appendTo(parent);
    var lcontDiv = $('<div class="slides-go-links"></div>').appendTo(goDiv);
    var linksDiv = $('<div class="slides-go-pad"></div>').appendTo(lcontDiv);
    goDiv.hide();
    var hoverDiv = $('<div class="slides-go-hover" />').appendTo(parent);
    hoverDiv.append($('<div><b>&raquo;</b></div>').css("position", "absolute")
                    .css("top", "50%").css("right", "5px"));

    // Set up hovering show/hide
    hoverDiv.mouseenter(function() {
        // XXX Disable mouse hiding
        // Hide previews
        jQuery.each(previews, function () {
            this.hide();
        });
        // Hide hover
        hoverDiv.fadeOut('fast');
        // Show go bar
        goDiv.fadeIn('fast');
    });
    goDiv.mouseleave(function() {
        goDiv.fadeOut('fast');
    });

    // Show hover when mouse is visible
    this._cursorHook.push(function (visible) {
        if (visible)
            hoverDiv.show();
        else
            hoverDiv.hide();
    });
    hoverDiv.hide();

    // Create per-slide previews and links
    jQuery.each(slides._slides, function (_, slide) {
        // Create the link
        var linkDiv = $('<div/>').text(slide.title).click(function() {
            slides.show(slide.n);
            event.stopPropagation();
        }).appendTo(linksDiv);

        if (!SLIDE_PREVIEWS)
            return;

        // Create the slide previews
        var previewDiv = $('<div class="slides-go-preview"></div>');
        previews[slide.n] = previewDiv;
        // Limit to 10 steps
        var limit = (slide.nsteps < 10) ? slide.nsteps : 10;
        for (var i = 0; i < limit; i++) {
            var step = Math.floor((i / limit) * slide.nsteps);
            var src = "thumbs/" + slide.n + "-" + step + ".png";
            $('<img/>').attr('src', src).appendTo(previewDiv).click(
                function(step) {return function () {
                    slides.show(slide.n, step, true);
                    event.stopPropagation();
                };}(step)
            );
        }
        // Limit width to 5 steps (so it could wrap to two rows)
        if (limit > 5)
            limit = 5;
        // XXX Hard-coded image width and margin
        previewDiv.width((128 + 5) * limit);
        goDiv.append(previewDiv);
        previewDiv.hide();

        linkDiv.mouseenter(function() {
            // Show the preview
            var pos = linkDiv.offset();
            // Adjust for padding (XXX hard-coded)
            pos.top -= 5;
            if (pos.top < 0)
                pos.top = 0;
            // Position to the right of the links div (can't use goDiv
            // because that includes the previews)
            pos.left = lcontDiv.width();
            // Avoid scrolling the page
            if (pos.top + previewDiv.height() > $(window).height())
                pos.top = $(window).height() - previewDiv.height();
            // XXX previewDiv.offset(pos) causes things to jump all over
            previewDiv.css("left", pos.left+"px").css("top", pos.top+"px");

            // Hide others
            jQuery.each(previews, function () {
                if (this !== previewDiv)
                    this.hide();
            });

            // Show this
            previewDiv.show();
        });
    });
};

function Master(under, over, cur) {
    this._under = under;
    this._over = over;
    this._cur = cur;

    var t = ((under && svgWrap(under).find("?title").maybeOne()) ||
             (over && svgWrap(over).find("?title").maybeOne()));
    if (t)
        this._title = $("tspan", t)[0];
    else
        this._title = null;
}

Master.prototype.show = function(title) {
    // Hide the current master
    var cur = this._cur.master;
    if (cur && cur._under !== this._under)
        cur._under.style.display = "none";
    if (cur && cur._over !== this._over)
        cur._over.style.display = "none";

    // Update slots
    if (this._title)
        this._title.textContent = title ? title : "";

    // Display this master
    if (this._under)
        this._under.style.display = "inline";
    if (this._over)
        this._over.style.display = "inline";
    this._cur.master = this;
};

function Slide(n, elt, master, transitions, cur, slides, prev) {
    this.n = n;
    this.elt = elt;
    this.master = master;
    this._transitions = transitions;
    this.cur = cur;
    // XXX Ugly.  We need this to call the pre- and post-animation
    // hooks.
    this._slides = slides;
    // The slide preceding this one (for inter-slide transitions)
    this._prev = prev;

    this.title = $(elt).inklabel();

    // Is this a multi-layer slide?
    this._layered = ($(elt).children(":inklayer").length > 0);

    this.orig = elt.cloneNode(true);

    // Prepare the slide up-front.  This marks the slide as non-dirty
    // so the first time we show it, we won't prepare it again.
    this._prepare();
}

Slide.prototype.show = function(step, end) {
    // Stop any running animation
    if (this.cur.anim)
        this.cur.anim.stop();

    // Rewind this slide to the beginning if necessary.  This is <=
    // and not just < because step could be animated, in which case we
    // need to rewind to the beginning of the animation.  If this
    // slide has already been prepared and never shown, this.step will
    // be 0, but this._dirty will be false.
    //
    // XXX This makes skipping forward through animations ironically
    // slow.  When possible we should just advance the animation.
    if (step <= this.step && this._dirty) {
        var newElt = this.orig.cloneNode(true);
        // XXX Might want to delay this to prevent flicker when moving
        // backwards on the same slide?
        this.elt.parentNode.replaceChild(newElt, this.elt);
        this.elt = newElt;
        this._prepare();
    }
    this._dirty = true;

    // Skip forward to the beginning of this step
    for (; this.step < step; this.step++)
        this.steps[this.step].eval(1);

    // If we want the end of this step, jump to it
    // XXX It would be nice to show the first frame of the animation
    // if !end before we show the slide, but eval(0) won't trigger the
    // reveal.
    if (end)
        this.steps[this.step].eval(1);

    if (step === 0 && !end && this._interslide
        && this.cur.slide === this._prev
        && this._prev.step === this._prev.nsteps - 1) {
        // Perform inter-slide transition, then hide the previous
        // slide, then do steps[0].
        this.cur.anim = Anim.seq(this._interslide,
                                 Action.unreveal(this._prev.elt),
                                 this.steps[0]);
        // Stabilize the inter-slide transition now, since we couldn't
        // safely prepare it in Slide._prepare.
        this.cur.anim.stabilize();
    } else {
        // Perform an immediate slide change (either because we came
        // from some slide besides the previous one or because there
        // is no inter-slide transition).

        // Hide other slides (we hide all other slides rather than
        // just the currently visible slide both to be paranoid and
        // because, if we stop an inter-slide transition before it
        // hides the previous slide, we still have to be sure to hide
        // that previous slide)
        for (var i = 0; i < this._slides._slides.length; i++)
            this._slides._slides[i].elt.style.display = "none";

        // Show this slide
        this.cur.anim = this.steps[this.step];
    }

    // Show this slide
    this.elt.style.display = "inline";
    this.cur.slide = this;
    this.master.show(this.title);

    // Begin animation of this step
    if (!end)
        this.cur.anim.start();
};

function flatten(arr) {
    var out = new Array(arr.length);
    var n = 0;
    var rec = function(arr) {
        for (var i = 0; i < arr.length; i++) {
            var val = arr[i];
            if (jQuery.isArray(val))
                rec(val);
            else
                out[n++] = val;
        }
    };
    rec(arr);
    return out;
}

Slide.prototype._prepare = function() {
    var othis = this;
    var i;

    // Construct this slide's animation.  We do this here rather than
    // the constructor because resetting the slide destroys the
    // original DOM objects, and thus requires reconstructing the
    // animation.
    var steps = [];
    var wrappedSlide = svgWrap(this.elt);
    var addSteps = function(reveal, transTitle, args) {
        var transition = othis._transitions[transTitle];
        var tsteps = [];
        if (transition) {
            tsteps = transition.apply(wrappedSlide, args);
            if ($.isArray(tsteps))
                tsteps = flatten(tsteps);
            else
                tsteps = [tsteps];
        }
        if (tsteps.length) {
            // Merge the reveal step with the first transition step
            tsteps[0] = reveal.seq(tsteps[0]);
        } else {
            // We always need at least the one reveal step
            tsteps = [reveal];
        }
        // Append all of the transition steps
        for (var i = 0; i < tsteps.length; i++)
            steps.push(tsteps[i]);
    };

    if (this._layered) {
        // Hide notes on non-current layers.  Notes on the current
        // layer get the default note visibility (via CSS cascading).
        function noteReveal(objs) {
            function noteRevealStep(t) {
                for (var i = 0; i < objs.length; ++i) {
                    if (t === 0)
                        objs[i].style.display = "none";
                    else
                        objs[i].style.display = "";
                }
            }
            return new Anim(noteRevealStep, 0);
        }

        var prevNotes = [];
        $(this.elt).children(":inklayer").each(function() {
            if ($(this).inklabel()[0] === ":")
                return;
            var reveal = Action.reveal([this]);
            var notes = $(".notes", this);
            if (notes.length)
                reveal = Anim.seq(noteReveal(notes), reveal);
            if (prevNotes.length)
                reveal = Anim.seq(noteReveal(prevNotes).reverse(), reveal);
            addSteps(reveal,
                     othis.title + "/" + $(this).inklabel(),
                     [svgWrap(this)]);
            prevNotes = notes;
        });
    } else {
        addSteps(Anim.none, this.title);
    }
    this.steps = steps;
    this.nsteps = steps.length;

    // Stabilize this slide's animation
    setupHackUndo = [];
    Anim.seq.apply(Anim, this.steps).stabilize();
    for (i = setupHackUndo.length - 1; i >= 0; i--)
        setupHackUndo[i]();
    setupHackUndo = null;

    // Force a full refresh at the end of each step, mostly to work
    // around Firefox's buggy filter bounding-box optimizations
    var forceRedraw = Action.forceRedraw([this.elt]);
    for (i = 0; i < this.nsteps; i++)
        this.steps[i] = this.steps[i].seq(forceRedraw);

    // If there is an inter-slide transition from the previous slide,
    // play it before step[0] when coming from the previous slide.
    var interslide = this._prev ?
        this._transitions[this._prev.title + "+" + this.title] : null;
    if (interslide) {
        // Note that, if the previous slide gets reconstructed, its
        // elt will be meaningless, but if we actually play this
        // animation, we'll also have reconstructed this slide (I
        // think), so this is safe.
        var wrappedPrev = svgWrap(this._prev.elt);
        // XXX Put previous slide in correct state while we construct
        // this animation.  This is terrible and suggests we should
        // stabilize the whole presentation instead of individual
        // slides.
        var prevAnim = Anim.seq.apply(Anim, this._prev.steps);
        prevAnim.eval(1);
        var tstep = interslide.call(null, wrappedPrev, wrappedSlide);
        // XXX If I'm resetting this slide because I'm moving from the
        // previous slide, this puts the previous slide in the wrong
        // state.
        prevAnim.eval(0);
        // Note that we can't safely stabilize the inter-slide
        // transition here because it may leave the previous slide in
        // the wrong state, so we stabilize it just before playing it.
        // XXX Does this mix poorly with reveals of subslides?
        this._interslide = tstep;
    }

    // Wrap pre- and post-animation hooks calls around each step.
    var preAnim = function() {
        if (othis._slides.preAnimHook) {
            console.log("pretAnimHook");
            try {
                othis._slides.preAnimHook();
            } catch (err) {
                console.error("preAnimHook exception: " + err);
            }
        }
    };
    var postAnim = function() {
        if (othis._slides.postAnimHook) {
            console.log("postAnimHook");
            try {
                othis._slides.postAnimHook();
            } catch (err) {
                console.error("postAnimHook exception: " + err);
            }
        }
    };
    for (i = 0; i < this.nsteps; i++)
        this.steps[i] = Anim.seq(new Anim(preAnim, 0),
                                 this.steps[i],
                                 new Anim(postAnim, 0));

    this.step = 0;
    this._dirty = false;
};

//////////////////////////////////////////////////////////////////
// Compatibility
//

if (!String.prototype.trim) {
    String.prototype.trim = function () {
        return this.replace(/^\s+|\s+$/g,'');
    };
}
