/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <iostream>
#include <boost/format.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// RenderEngineWindow
//------------------------------------------------------------------------------

RenderEngineWindow::RenderEngineWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Render Engine") {
	typeTable
		.Add("PATHOCL", 0)
		.Add("LIGHTCPU", 1)
		.Add("PATHCPU", 2)
		.Add("BIDIRCPU", 3)
		.Add("BIDIRVMCPU", 4)
		.Add("RTPATHOCL", 5)
		.Add("BIASPATHCPU", 6)
		.Add("BIASPATHOCL", 7)
		.Add("RTBIASPATHOCL", 8)
		.SetDefault("PATHCPU");
}

Properties RenderEngineWindow::GetAllRenderEngineProperties(const Properties &cfgProps) const {
	Properties props = 
			cfgProps.GetAllProperties("renderengine") <<
			cfgProps.GetAllProperties("path") <<
			cfgProps.GetAllProperties("light") <<
			cfgProps.GetAllProperties("bidirvm") <<
			cfgProps.GetAllProperties("rtpath") <<
			cfgProps.GetAllProperties("biaspath") << 
			cfgProps.GetAllProperties("tile") <<
			cfgProps.GetAllProperties("native.threads.count") <<
			cfgProps.GetAllProperties("opencl.task.count");

	if (props.IsDefined("tile.multipass.convergencetest.threshold")) {
		const float t = props.Get("tile.multipass.convergencetest.threshold").Get<float>();
		props.Delete("tile.multipass.convergencetest.threshold");
		props << Property("tile.multipass.convergencetest.threshold256")(t * 256.0);
	}

	return props;
}

void RenderEngineWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetAllRenderEngineProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("RenderEngine parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetAllRenderEngineProperties(config->GetProperties());
	}
}

void RenderEngineWindow::ParseObjectProperties(const Properties &props) {
	app->EditRenderConfig(GetAllRenderEngineProperties(props));
}

void RenderEngineWindow::PathGUI(Properties &props, bool &modifiedProps) {
	bool bval;
	float fval;
	int ival;

	ival = props.Get("path.maxdepth").Get<int>();
	if (ImGui::InputInt("Maximum recursion depth", &ival)) {
		props.Set(Property("path.maxdepth")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.maxdepth");

	ival = props.Get("path.russianroulette.depth").Get<int>();
	if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
		props.Set(Property("path.russianroulette.depth")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.russianroulette.depth");

	fval = props.Get("path.russianroulette.cap").Get<float>();
	if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
		props.Set(Property("path.russianroulette.cap")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.russianroulette.cap");

	fval = props.Get("path.clamping.variance.maxvalue").Get<float>();
	if (ImGui::InputFloat("Variance clamping", &fval)) {
		props.Set(Property("path.clamping.variance.maxvalue")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.clamping.variance.maxvalue");

	fval = props.Get("path.clamping.pdf.value").Get<float>();
	if (ImGui::InputFloat("PDF clamping", &fval)) {
		props.Set(Property("path.clamping.pdf.value")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.clamping.pdf.value");

	bval = props.Get("path.fastpixelfilter.enable").Get<float>();
	if (ImGui::Checkbox("Use fast pixel filter", &bval)) {
		props.Set(Property("path.fastpixelfilter.enable")(bval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.clamping.pdf.value");
}

void RenderEngineWindow::BiasPathGUI(Properties &props, bool &modifiedProps) {
	bool bval;
	float fval;
	int ival;

	ival = props.Get("biaspath.pathdepth.total").Get<int>();
	if (ImGui::InputInt("Maximum total recursion depth", &ival)) {
		props.Set(Property("biaspath.pathdepth.total")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.pathdepth.total");

	ival = props.Get("biaspath.pathdepth.diffuse").Get<int>();
	if (ImGui::InputInt("Maximum diffuse recursion depth", &ival)) {
		props.Set(Property("biaspath.pathdepth.diffuse")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.pathdepth.diffuse");

	ival = props.Get("biaspath.pathdepth.glossy").Get<int>();
	if (ImGui::InputInt("Maximum glossy recursion depth", &ival)) {
		props.Set(Property("biaspath.pathdepth.glossy")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.pathdepth.glossy");

	ival = props.Get("biaspath.pathdepth.specular").Get<int>();
	if (ImGui::InputInt("Maximum specular recursion depth", &ival)) {
		props.Set(Property("biaspath.pathdepth.specular")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.pathdepth.specular");

	ival = props.Get("biaspath.sampling.aa.size").Get<int>();
	if (ImGui::InputInt(("x" + ToString(ival) + " Anti-aliasing").c_str(), &ival)) {
		props.Set(Property("biaspath.sampling.aa.size")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.sampling.aa.size");

	ival = props.Get("biaspath.sampling.diffuse.size").Get<int>();
	if (ImGui::InputInt(("x" + ToString(ival) + " Diffuse samples").c_str(), &ival)) {
		props.Set(Property("biaspath.sampling.diffuse.size")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.sampling.diffuse.size");

	ival = props.Get("biaspath.sampling.glossy.size").Get<int>();
	if (ImGui::InputInt(("x" + ToString(ival) + " Glossy samples").c_str(), &ival)) {
		props.Set(Property("biaspath.sampling.glossy.size")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.sampling.glossy.size");

	ival = props.Get("biaspath.sampling.specular.size").Get<int>();
	if (ImGui::InputInt(("x" + ToString(ival) + " Specular samples").c_str(), &ival)) {
		props.Set(Property("biaspath.sampling.specular.size")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.sampling.specular.size");

	ival = props.Get("biaspath.sampling.directlight.size").Get<int>();
	if (ImGui::InputInt(("x" + ToString(ival) + " Direct light samples").c_str(), &ival)) {
		props.Set(Property("biaspath.sampling.directlight.size")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.sampling.directlight.size");

	fval = props.Get("biaspath.clamping.variance.maxvalue").Get<float>();
	if (ImGui::InputFloat("Variance clamping", &fval)) {
		props.Set(Property("biaspath.clamping.variance.maxvalue")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.clamping.variance.maxvalue");

	fval = props.Get("biaspath.clamping.pdf.value").Get<float>();
	if (ImGui::InputFloat("PDF clamping", &fval)) {
		props.Set(Property("biaspath.clamping.pdf.value")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.clamping.pdf.value");

	fval = props.Get("biaspath.lights.lowthreshold").Get<float>();
	if (ImGui::InputFloat("Light low intensity threshold", &fval)) {
		props.Set(Property("biaspath.lights.lowthreshold")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.lights.lowthreshold");
	
	fval = props.Get("biaspath.lights.nearstart").Get<float>();
	if (ImGui::InputFloat("Light distance threshold", &fval)) {
		props.Set(Property("biaspath.lights.nearstart")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.lights.nearstart");

	ival = props.Get("biaspath.lights.firstvertexsamples").Get<int>();
	if (ImGui::InputInt("First hit direct light samples", &ival)) {
		props.Set(Property("biaspath.lights.firstvertexsamples")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("biaspath.lights.firstvertexsamples");

	if (props.IsDefined("tile.size.x") || props.IsDefined("tile.size.y")) {
		ival = props.Get("tile.size.x").Get<int>();
		if (ImGui::SliderInt("Tile width", &ival, 8, 512, "%.0f pixels")) {
			props.Set(Property("tile.size.x")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.size.x");

		ival = props.Get("tile.size.y").Get<int>();
		if (ImGui::SliderInt("Tile height", &ival, 8, 512, "%.0f pixels")) {
			props.Set(Property("tile.size.y")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.size.y");
	} else {
		ival = props.Get("tile.size").Get<int>();
		if (ImGui::SliderInt("Tile size", &ival, 8, 512, "%.0f pixels")) {
			props.Set(Property("tile.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.size");

		if (ImGui::Button("Separate tile horizontal and vertical size")) {
			props.Set(Property("tile.size.x")(32));
			props.Set(Property("tile.size.y")(64));
			modifiedProps = true;
		}
	}

	bval = props.Get("tile.multipass.enable").Get<bool>();
	if (ImGui::Checkbox("Multi-pass rendering", &bval)) {
		props.Set(Property("tile.multipass.enable")(bval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("tile.multipass.enable");

	if (props.Get("tile.multipass.enable").Get<bool>()) {
		fval = props.Get("tile.multipass.convergencetest.threshold256").Get<float>();
		if (ImGui::SliderFloat("Convergence test threshold", &fval, 0.f, 256.f)) {
			props.Set(Property("tile.multipass.convergencetest.threshold256")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.multipass.convergencetest.threshold256");

		fval = props.Get("tile.multipass.convergencetest.threshold.reduction").Get<float>();
		if (ImGui::InputFloat("Convergence test threshold reduction", &fval)) {
			props.Set(Property("tile.multipass.convergencetest.threshold.reduction")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.multipass.convergencetest.threshold.reduction");

		ival = props.Get("tile.multipass.convergencetest.warmup.count").Get<int>();
		if (ImGui::SliderInt("Convergence test warmup", &ival, 8, 512, "%.0f samples/pixel")) {
			props.Set(Property("tile.multipass.convergencetest.warmup.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.multipass.convergencetest.warmup.count");
	}
}

void RenderEngineWindow::PathOCLGUI(Properties &props, bool &modifiedProps) {
	PathGUI(props, modifiedProps);

	bool bval;
	int ival;

	bval = props.Get("path.pixelatomics.enable").Get<float>();
	if (ImGui::Checkbox("Use pixel atomics", &bval)) {
		props.Set(Property("path.pixelatomics.enable")(bval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.pixelatomics.enable");

	bool autoOCLTaskCount = (props.Get("opencl.task.count").Get<string>() == "AUTO") ? true : false;
	if (ImGui::Checkbox("Automatic OpenCL task count", &autoOCLTaskCount)) {
		if (autoOCLTaskCount)
			props.Set(Property("opencl.task.count")("AUTO"));
		else
			props.Set(Property("opencl.task.count")(128 * 1024 * 1024));

		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("opencl.task.count");

	if (!autoOCLTaskCount) {
		ival = props.Get("opencl.task.count").Get<int>();
		if (ImGui::InputInt("Threads count", &ival, 8 * 1024, 128 * 1024)) {
			props.Set(Property("opencl.task.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("opencl.task.count");
	}
}

void RenderEngineWindow::BiDirGUI(Properties &props, bool &modifiedProps) {
	float fval;
	int ival;

	ival = props.Get("path.maxdepth").Get<int>();
	if (ImGui::InputInt("Maximum eye path recursion depth", &ival)) {
		props.Set(Property("path.maxdepth")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.maxdepth");

	ival = props.Get("light.maxdepth").Get<int>();
	if (ImGui::InputInt("Maximum light path recursion depth", &ival)) {
		props.Set(Property("light.maxdepth")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("light.maxdepth");

	ival = props.Get("path.russianroulette.depth").Get<int>();
	if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
		props.Set(Property("path.russianroulette.depth")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.russianroulette.depth");

	fval = props.Get("path.russianroulette.cap").Get<float>();
	if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
		props.Set(Property("path.russianroulette.cap")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("path.russianroulette.cap");

	ival = props.Get("native.threads.count").Get<int>();
	if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
		props.Set(Property("native.threads.count")(ival));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("native.threads.count");
}

bool RenderEngineWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//------------------------------------------------------------------
	// renderengine.type
	//------------------------------------------------------------------

	const string currentRenderEngineType = props.Get(Property("renderengine.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentRenderEngineType);

	if (ImGui::Combo("Render Engine type", &typeIndex, typeTable.GetTagList())) {
		props.Clear();

		props << Property("renderengine.type")(typeTable.GetTag(typeIndex));

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("renderengine.type");

	//------------------------------------------------------------------
	// RTBIASPATHOCL
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("RTBIASPATHOCL")) {
		BiasPathGUI(props, modifiedProps);

		int ival;

		ival = props.Get("biaspathocl.devices.maxtiles").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, 32)) {
			props.Set(Property("biaspathocl.devices.maxtiles")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspathocl.devices.maxtiles");

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		/*ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor")) {
			// TODO
		}*/
	}

	//------------------------------------------------------------------
	// BIASPATHOCL
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIASPATHOCL")) {
		BiasPathGUI(props, modifiedProps);

		int ival;

		ival = props.Get("biaspathocl.devices.maxtiles").Get<int>();
		if (ImGui::SliderInt("Max. number of tiles for each device", &ival, 1, 32)) {
			props.Set(Property("biaspathocl.devices.maxtiles")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspathocl.devices.maxtiles");

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		/*ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor")) {
			// TODO
		}*/
	}

	//------------------------------------------------------------------
	// BIASPATHCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIASPATHCPU")) {
		BiasPathGUI(props, modifiedProps);

		int ival;

		ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//------------------------------------------------------------------
	// RTPATHOCL
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("RTPATHOCL")) {
		PathOCLGUI(props, modifiedProps);

		float fval;
		int ival;

		ival = props.Get("rtpath.miniterations").Get<float>();
		if (ImGui::InputInt("Min. pass count per frame", &ival)) {
			props.Set(Property("rtpath.miniterations")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("rtpath.miniterations");

		ival = props.Get("rtpath.displaydevice.index").Get<float>();
		if (ImGui::InputInt("OpenCL device index to use for display tasks", &ival)) {
			props.Set(Property("rtpath.displaydevice.index")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("rtpath.displaydevice.index");

		fval = props.Get("rtpath.blur.timewindow").Get<float>();
		if (ImGui::InputFloat("Blur time length in secs", &fval)) {
			props.Set(Property("rtpath.blur.timewindow")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("rtpath.blur.timewindow");

		fval = props.Get("rtpath.blur.mincap").Get<float>();
		if (ImGui::InputFloat("Blur min. weight", &fval)) {
			props.Set(Property("rtpath.blur.mincap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("rtpath.blur.mincap");

		fval = props.Get("rtpath.blur.maxcap").Get<float>();
		if (ImGui::InputFloat("Blur max. weight", &fval)) {
			props.Set(Property("rtpath.blur.maxcap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("rtpath.blur.maxcap");

		fval = props.Get("rtpath.ghosteffect.intensity").Get<float>();
		if (ImGui::InputFloat("Ghost effect intensity", &fval)) {
			props.Set(Property("rtpath.ghosteffect.intensity")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("rtpath.ghosteffect.intensity");

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		/*ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor")) {
			// TODO
		}*/
	}

	//------------------------------------------------------------------
	// PATHOCL
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("PATHOCL")) {
		PathOCLGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		/*ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor")) {
			// TODO
		}*/
	}

	//------------------------------------------------------------------
	// PATHCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("PATHCPU")) {
		PathGUI(props, modifiedProps);

		int ival;

		ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//------------------------------------------------------------------
	// BIDIRCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIDIRCPU")) {
		BiDirGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//------------------------------------------------------------------
	// BIDIRVMCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIDIRVMCPU")) {
		BiDirGUI(props, modifiedProps);

		float fval;
		int ival;

		ival = props.Get("bidirvm.lightpath.count").Get<int>();
		if (ImGui::SliderInt("Light path count for each pass", &ival, 256, 128 * 1024)) {
			props.Set(Property("bidirvm.lightpath.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("bidirvm.lightpath.count");

		fval = props.Get("bidirvm.startradius.scale").Get<float>();
		if (ImGui::InputFloat("Start radius scale", &fval)) {
			props.Set(Property("bidirvm.startradius.scale")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("bidirvm.startradius.scale");

		fval = props.Get("bidirvm.alpha").Get<float>();
		if (ImGui::SliderFloat("Radius reduction", &fval, 0.f, 1.f)) {
			props.Set(Property("bidirvm.alpha")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("bidirvm.alpha");

		ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//------------------------------------------------------------------
	// LIGHTCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("LIGHTCPU")) {
		float fval;
		int ival;

		ival = props.Get("light.maxdepth").Get<int>();
		if (ImGui::InputInt("Maximum light path recursion depth", &ival)) {
			props.Set(Property("light.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("light.maxdepth");

		ival = props.Get("light.russianroulette.depth").Get<int>();
		if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
			props.Set(Property("light.russianroulette.depth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("light.russianroulette.depth");

		fval = props.Get("light.russianroulette.cap").Get<float>();
		if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
			props.Set(Property("light.russianroulette.cap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("light.russianroulette.cap");

		ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	return false;
}