/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of all CSS property aliases with data about them, for preprocessing
 */

/******

  This file contains the list of all CSS properties that are just
  aliases for other properties (e.g., for when we temporarily continue
  to support a prefixed property after adding support for its unprefixed
  form).  It is designed to be used as inline input through the magic of
  C preprocessing.  All entries must be enclosed in the appropriate
  CSS_PROP_ALIAS macro which will have cruel and unusual things done to
  it.

  The arguments to CSS_PROP_ALIAS are:

  -. 'aliasname' entries represent a CSS property name and *must* use
  only lowercase characters.

  -. 'id' should be the same as the 'id' field in nsCSSPropList.h for
  the property that 'aliasname' is being aliased to.

  -. 'method' is the CSS2Properties property name.  Unlike
  nsCSSPropList.h, prefixes should just be included in this file (rather
  than needing the CSS_PROP_DOMPROP_PREFIXED(prop) macro).

  -. 'pref' is the name of a pref that controls whether the property
  is enabled.  The property is enabled if 'pref' is an empty string,
  or if the boolean property whose name is 'pref' is set to true.

 ******/

CSS_PROP_ALIAS(-moz-transform, transform, MozTransform, "")
CSS_PROP_ALIAS(-moz-transform-origin, transform_origin, MozTransformOrigin, "")
CSS_PROP_ALIAS(-moz-perspective-origin, perspective_origin, MozPerspectiveOrigin, "")
CSS_PROP_ALIAS(-moz-perspective, perspective, MozPerspective, "")
CSS_PROP_ALIAS(-moz-transform-style, transform_style, MozTransformStyle, "")
CSS_PROP_ALIAS(-moz-backface-visibility, backface_visibility, MozBackfaceVisibility, "")
CSS_PROP_ALIAS(-moz-border-image, border_image, MozBorderImage, "")
CSS_PROP_ALIAS(-moz-transition, transition, MozTransition, "")
CSS_PROP_ALIAS(-moz-transition-delay, transition_delay, MozTransitionDelay, "")
CSS_PROP_ALIAS(-moz-transition-duration, transition_duration, MozTransitionDuration, "")
CSS_PROP_ALIAS(-moz-transition-property, transition_property, MozTransitionProperty, "")
CSS_PROP_ALIAS(-moz-transition-timing-function, transition_timing_function, MozTransitionTimingFunction, "")
CSS_PROP_ALIAS(-moz-animation, animation, MozAnimation, "")
CSS_PROP_ALIAS(-moz-animation-delay, animation_delay, MozAnimationDelay, "")
CSS_PROP_ALIAS(-moz-animation-direction, animation_direction, MozAnimationDirection, "")
CSS_PROP_ALIAS(-moz-animation-duration, animation_duration, MozAnimationDuration, "")
CSS_PROP_ALIAS(-moz-animation-fill-mode, animation_fill_mode, MozAnimationFillMode, "")
CSS_PROP_ALIAS(-moz-animation-iteration-count, animation_iteration_count, MozAnimationIterationCount, "")
CSS_PROP_ALIAS(-moz-animation-name, animation_name, MozAnimationName, "")
CSS_PROP_ALIAS(-moz-animation-play-state, animation_play_state, MozAnimationPlayState, "")
CSS_PROP_ALIAS(-moz-animation-timing-function, animation_timing_function, MozAnimationTimingFunction, "")
