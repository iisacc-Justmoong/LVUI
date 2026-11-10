#pragma once
#include <LVUI/Tooltip.h>
namespace lvui {
template<template<class>class Engine>using TooltipOutlineBinding=Engine<TooltipModelPolicy>;
} // namespace lvui
