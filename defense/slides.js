// XXX Something is wrong if I back up over the first inter-slide
// transition

function skewed(anims, delay) {
    var sa = [];
    for (var i = 0; i < anims.length; i++) {
        var d = typeof(delay) === 'number' ? delay * i : delay[i];
        sa[i] = Anim.seq(new Anim(nothing, d), anims[i]);
    }
    return Anim.par.apply(Anim, sa);
}

function fadeBy(objects, tSel, objDuration, totalDuration) {
    if (totalDuration === undefined)
        totalDuration = 1;
    // Compute time selectors
    var sels = [];
    for (var i = 0; i < objects.length; i++)
        sels.push(tSel(objects[i]));
    // Compute delays
    var fsels = sels.filter(function (x) {return x !== undefined});
    var min = Math.min.apply(Math, fsels);
    var span = Math.max.apply(Math, fsels) - min;
    var delays = [];
    for (var i = 0; i < objects.length; i++) {
        if (sels[i] === undefined)
            delays[i] = 0;
        else
            delays[i] = ((sels[i] - min) / span) * (totalDuration - objDuration);
    }
    // Create fades
    var fades = [];
    for (var i = 0; i < objects.length; i++)
        fades.push(Action.fade(objects[i]).scaleTo(objDuration));
    return skewed(fades, delays);
}

function getTspanY(tspan) {
    if (!tspan.getNumberOfChars())
        return undefined;
    return tspan.getStartPositionOfChar(0).y;
}

// Speed for quick transitions that smooth flow, but aren't
// first-class animations.
Anim.prototype.quick = function() {
    return this.scaleTo(0.25);
};

var transitions = {
    "Current approach to scalable software development/Papers" : function() {
        var _ = this.finder;
        var timeline = autoHandle(_("?timeline")[0]);
        return Anim.par(
            fadeBy(_("?pub"),
                   function (obj) {return autoHandle(obj).pos[1];},
                   0.6),
            Action.fade(_("?timeline")),
            // XXX Ugh.  Better segBetween?  Better point abstraction?
            Action.translateAbs(timeline, Inter.seg(
                timeline.pos[0] - 40, timeline.pos[1],
                timeline.pos[0], timeline.pos[1])).easeOutQuad()).quick();
    },

    "Current approach to scalable software development/Cycle" : function() {
        var _ = this.finder;
        var center = new BBoxHandle(_("?workload")[0]);
        function edgeReveal(name) {
            return Anim.par(
                Action.fade(_(name)),
                Action.rotate(_(name+"-edge"), Inter.expr("-30*(1-t)"),
                              center).easeOutExpo()
            ).quick();
        }
        return [Action.reveal(_("?workload")),
                Action.fade(_("?plot")).quick(),
                edgeReveal("?diff"),
                edgeReveal("?fix"),
                edgeReveal("?repeat")];
    },

    // Note the trailing space
    "Current approach to scalable software development " : function() {
        var _ = this.finder;
        // var realSpan = $("tspan", _("?disadvantages")[0])[3];
        // return [Anim.none,
        //         Anim.par(Action.reveal(_("?bold")),
        //                  Action.unreveal(realSpan))];
        return [Anim.none,
                Action.fadeTo(_("?fade"), 0.25).quick()];
    },

    // "Current approach to scalable software development+Problem" :
    // function(a, b) {
    //     var _a = a.finder, _b = b.finder;
    //     return Anim.par(
    //         Anim.seq(Action.fadeOut(_a("?Papers, ?Bang, ?tab")),
    //                  Action.fade($("text", b))).scaleTo(1),
    //         Anim.seq(
    //             Action.translateAbs(
    //                 _a("?cycle"),
    //                 Inter.segBetween(
    //                     // XXX Why do I have to detach b?  If I don't,
    //                     // then something doesn't have an
    //                     // ownerSVGElement.
    //                     autoHandle(_a("?cycle")[0]).detach(),
    //                     autoHandle(_b("?cycle")[0]).detach())).easeInOutQuad(),
    //             Action.reveal(_b("?cycle")))).quick();
    // },

    // "Workload-driven approach to fixing bottlenecks" : function() {
    //     var _ = this.finder;
    //     return [Anim.none,
    //             Action.reveal(_("?diff")),
    //             Action.reveal(_("?fix")),
    //             Action.reveal(_("?check")),
    //             Action.reveal(_("?repeat"))];
    // },

    "Approach: Interface-driven scalability/lowest" : function() {
        var _ = this.finder;
        return [Anim.none,
                Action.fade(_("?lowest-ex")).scaleTo(0),
                Anim.par(Action.fadeOut(_("?lowest-q, ?lowest-ex")),
                         Action.fade(_("?lowest-x"))).quick()];
    },

    "Approach: Interface-driven scalability/any" : function() {
        var _ = this.finder;
        return [Anim.none,
                Anim.par(Action.fadeOut(_("?any-q, ?any-ex")),
                         Action.fade(_("?any-answer"))).quick()];
    },

    "What scales on today's multicores?/Examples" : function() {
        var _ = this.finder;
        return [Action.fade(_("?rr")).quick(),
                Anim.par(Action.fade(_("?rw")),
                         Action.fadeOut(_("?rr"))).quick()];
    },

    "What scales on today's multicores?/Def" : function() {
        var _ = this.finder;
        return [Action.fadeOut(_("?rw")).quick()];
    },

    "Interface scalability example/Any FD" : function() {
        var _ = this.finder;
        return Anim.par(
            Action.fadeOut(_("?serial")), Action.fade(_("?par"))).quick();
    },

    // "Goal: Interface-driven approach/Advantages" : function(subslide) {
    //     var _ = subslide.finder;
    //     return fadeBy($("tspan", subslide[0]), getTspanY, 0.6).quick();
    // },

    // "Modern multicore hardware/Hardware" : function() {
    //     var _ = this.finder;
    //     return [Anim.none,
    //             // XXX Glitchy
    //             Action.reveal(_("?cache, ?local")),
    //             Action.reveal(_("?remote"))];
    // },

    // "SIM commutativity/Props" : function() {
    //     var _ = this.finder;
    //     return fadeBy($("tspan", _("?props")[0]), getTspanY, 0.6).quick();
    // },

    "Example: Reference counter/R2p" : function() {
        var _ = this.finder;
        return [Anim.seq(
                    Action.fade(_("?hide-num")).par(
                        Action.fadeOut(_("?r2"))),
                    Action.fade(_("?ok, ?r2p")).faster()),
               ];
    },

    "Commutativity" : function() {
        var _ = this.finder;
        var ltr = Inter.seg(55, 0).easeInOutQuad().seq(
            Inter.seg(55, 0, 0, 0).easeInOutQuad());
        var rtl = Inter.seg(-55, 0).easeInOutQuad().seq(
            Inter.seg(-55, 0, 0, 0).easeInOutQuad());
        return [Anim.none,
                Action.fade(_("?y, ?threads")).quick(),
                Action.fade(_("?x")).quick(),
                Anim.par(
                    Action.translateRel(_("?i3"), ltr),
                    Action.translateRel(_("?i4"), rtl)),
                Action.fade(_("?z")).quick(),
                ];
    },

    "Formalizing the rule/Bang" : function(subslide) {
        return Action.fade(subslide[0]).quick();
    },

    // "Example of using the rule: mmap/Common" : function() {
    //     var _ = this.finder;
    //     return [Anim.fade(_("?common")).quick()];
    // },

    "Input: Symbolic model+Commutativity conditions" :
    function(a, b) {
        var _a = a.finder, _b = b.finder;
        var dist = (autoHandle(_a("?listing")[0]).pos[1] -
                    autoHandle(_b("?listing")[0]).pos[1])
        var bh = autoHandle(_b("?slide")[0]);
        return Anim.par(
            Action.unreveal(a[0]),
            Action.fade(_b("?arrow")).scaleTo(0.5),
            Action.translateAbs(
                bh,
                Inter.seg(bh.pos[0], bh.pos[1] + dist,
                          bh.pos[0], bh.pos[1])).easeInOutQuad()).quick();
    },

    "Commutativity conditions+Test cases" :
    function(a, b) {
        var _a = a.finder, _b = b.finder;
        var dist = (autoHandle(_a("?listing")[0]).pos[1] -
                    autoHandle(_b("?listing")[0]).pos[1])
        var bh = autoHandle(_b("?slide")[0]);
        return Anim.par(
            Action.unreveal(a[0]),
            Action.fadeOut(_b("?arrow-out")).scaleTo(0.5),
            Action.fade(_b("?arrow-in")).scaleTo(0.5),
            Action.translateAbs(
                bh,
                Inter.seg(bh.pos[0], bh.pos[1] + dist,
                          bh.pos[0], bh.pos[1])).easeInOutQuad()).quick();
    },


    "Test cases+Output: Conflicting cache lines" :
    function(a, b) {
        var _a = a.finder, _b = b.finder;
        var dist = (autoHandle(_a("?listing")[0]).pos[1] -
                    autoHandle(_b("?listing")[0]).pos[1])
        var bh = autoHandle(_b("?slide")[0]);
        return Anim.par(
            Action.unreveal(a[0]),
            Action.fadeOut(_b("?arrow-out")).scaleTo(0.5),
            Action.translateAbs(
                bh,
                Inter.seg(bh.pos[0], bh.pos[1] + dist,
                          bh.pos[0], bh.pos[1])).easeInOutQuad()).quick();
    },

    "Commuter finds non-scalable cases in Linux/Mask" : function(subslide) {
        return [Action.fade(subslide).quick(),
                Action.fadeOut(subslide).quick(),
               ];
    },

    "Commutative operations can be made to scale/Mask" : function(subslide) {
        return [Action.fade(subslide).quick()];
    },
};

var slides;

$(document).ready(function () {
    slides = new Slides($("body"), "slides.svg", transitions, false);
    slides.onload = function() {
        slides.bindEvents();
    };
});
