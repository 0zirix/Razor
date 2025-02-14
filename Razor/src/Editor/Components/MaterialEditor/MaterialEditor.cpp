#include "rzpch.h"
#include "imgui.h"
#include "Editor/Editor.h"
#include "Editor/Components/EditorViewport.h"
#include "MaterialEditor.h"

#define sizeof_array(t) (sizeof(t) / sizeof(t[0]))
const float NODE_SLOT_RADIUS = 5.0f;
const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);
#define MAX_CONNECTION_COUNT 32


static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error %d: %s\n", error, description);
}


enum ConnectionType
{
	ConnectionType_Color,
	ConnectionType_Vec3,
	ConnectionType_Float,
	ConnectionType_Int,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectionDesc
{
	const char* name;
	ConnectionType type;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct NodeType
{
	const char* name;
	ConnectionDesc inputConnections[MAX_CONNECTION_COUNT];
	ConnectionDesc outputConnections[MAX_CONNECTION_COUNT];
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Connection
{
	ImVec2 pos;
	ConnectionDesc desc;

	inline Connection()
	{
		pos.x = pos.y = 0.0f;
		input = 0;
	}

	union {
		float v3[3];
		float v;
		int i;
	};

	struct Connection* input;
	std::vector<Connection*> output;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Node types

static struct NodeType s_nodeTypes[] =
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Math

	{
		"Multiply",
		// Input connections
		{
			{ "Input1", ConnectionType_Float },
			{ "Input2", ConnectionType_Float },
		},
		// Output
		{
			{ "Out", ConnectionType_Float },
		},
	},

	{
		"Add",
		// Input connections
		{
			{ "Input1", ConnectionType_Float },
			{ "Input2", ConnectionType_Float },
		},
		// Output
		{
			{ "Out", ConnectionType_Float },
		},
	},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct NodeGraph
{
	ImVec2 pos;
	ImVec2 size;
	int id;
	const char* name;
	std::vector<Connection*> inputConnections;
	std::vector<Connection*> outputConnections;
};

static uint32_t s_id = 0;


static void setupConnections(std::vector<Connection*>& connections, ConnectionDesc* connectionDescs)
{
	for (int i = 0; i < MAX_CONNECTION_COUNT; ++i)
	{
		const ConnectionDesc& desc = connectionDescs[i];

		if (!desc.name)
			break;

		Connection* con = new Connection;
		con->desc = desc;

		connections.push_back(con);
	}
}

static NodeGraph* createNodeFromType(ImVec2 pos, NodeType* nodeType)
{
	NodeGraph* node = new NodeGraph;
	node->id = s_id++;
	node->name = nodeType->name;

	ImVec2 titleSize = ImGui::CalcTextSize(node->name);

	titleSize.y *= 3;

	setupConnections(node->inputConnections, nodeType->inputConnections);
	setupConnections(node->outputConnections, nodeType->outputConnections);

	// Calculate the size needed for the whole box

	ImVec2 inputTextSize(0.0f, 0.0f);
	ImVec2 outputText(0.0f, 0.0f);

	for (Connection* c : node->inputConnections)
	{
		ImVec2 textSize = ImGui::CalcTextSize(c->desc.name);
		inputTextSize.x = std::max<float>(textSize.x, inputTextSize.x);

		c->pos = ImVec2(0.0f, titleSize.y + inputTextSize.y + textSize.y / 2.0f);

		inputTextSize.y += textSize.y;
		inputTextSize.y += 4.0f;		// size between text entries
	}

	inputTextSize.x += 40.0f;

	// max text size + 40 pixels in between

	float xStart = inputTextSize.x;

	// Calculate for the outputs

	for (Connection* c : node->outputConnections)
	{
		ImVec2 textSize = ImGui::CalcTextSize(c->desc.name);
		inputTextSize.x = std::max<float>(xStart + textSize.x, inputTextSize.x);
	}

	node->pos = pos;
	node->size.x = inputTextSize.x;
	node->size.y = inputTextSize.y + titleSize.y;

	inputTextSize.y = 0.0f;

	// set the positions for the output nodes when we know where the place them

	for (Connection* c : node->outputConnections)
	{
		ImVec2 textSize = ImGui::CalcTextSize(c->desc.name);

		c->pos = ImVec2(node->size.x, titleSize.y + inputTextSize.y + textSize.y / 2.0f);

		inputTextSize.y += textSize.y;
		inputTextSize.y += 4.0f;		// size between text entries
	}

	// calculate the size of the node depending on nuber of connections

	return node;
}

NodeGraph* createNodeFromName(ImVec2 pos, const char* name)
{
	for (int i = 0; i < (int)sizeof_array(s_nodeTypes); ++i)
	{
		if (!strcmp(s_nodeTypes[i].name, name))
			return createNodeFromType(pos, &s_nodeTypes[i]);
	}

	return 0;
}

enum DragState
{
	DragState_Default,
	DragState_Hover,
	DragState_BeginDrag,
	DragState_Draging,
	DragState_Connect,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DragNode
{
	ImVec2 pos;
	Connection* con;
};

static DragNode s_dragNode;
static DragState s_dragState = DragState_Default;

static std::vector<NodeGraph*> s_nodes;


void drawHermite(ImDrawList* drawList, ImVec2 p1, ImVec2 p2, int STEPS)
{
	ImVec2 t1 = ImVec2(+80.0f, 0.0f);
	ImVec2 t2 = ImVec2(+80.0f, 0.0f);

	for (int step = 0; step <= STEPS; step++)
	{
		float t = (float)step / (float)STEPS;
		float h1 = +2 * t * t * t - 3 * t * t + 1.0f;
		float h2 = -2 * t * t * t + 3 * t * t;
		float h3 = t * t * t - 2 * t * t + t;
		float h4 = t * t * t - t * t;
		drawList->PathLineTo(ImVec2(h1 * p1.x + h2 * p2.x + h3 * t1.x + h4 * t2.x, h1 * p1.y + h2 * p2.y + h3 * t1.y + h4 * t2.y));
	}

	drawList->PathStroke(ImColor(200, 200, 100), false, 3.0f);
}

static bool isConnectorHovered(Connection* c, ImVec2 offset)
{
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	ImVec2 conPos = offset + c->pos;

	float xd = mousePos.x - conPos.x;
	float yd = mousePos.y - conPos.y;

	return ((xd * xd) + (yd * yd)) < (NODE_SLOT_RADIUS * NODE_SLOT_RADIUS);
}


static Connection* getHoverCon(ImVec2 offset, ImVec2* pos)
{
	for (NodeGraph* node : s_nodes)
	{
		ImVec2 nodePos = node->pos + offset;

		for (Connection* con : node->inputConnections)
		{
			if (isConnectorHovered(con, nodePos))
			{
				*pos = nodePos + con->pos;
				return con;
			}
		}

		for (Connection* con : node->outputConnections)
		{
			if (isConnectorHovered(con, nodePos))
			{
				*pos = nodePos + con->pos;
				return con;
			}
		}
	}

	s_dragNode.con = 0;
	return 0;
}



void updateDraging(ImVec2 offset)
{
	switch (s_dragState)
	{
	case DragState_Default:
	{
		ImVec2 pos;
		Connection* con = getHoverCon(offset, &pos);

		if (con)
		{
			s_dragNode.con = con;
			s_dragNode.pos = pos;
			s_dragState = DragState_Hover;
			return;
		}

		break;
	}

	case DragState_Hover:
	{
		ImVec2 pos;
		Connection* con = getHoverCon(offset, &pos);

		// Make sure we are still hovering the same node

		if (con != s_dragNode.con)
		{
			s_dragNode.con = 0;
			s_dragState = DragState_Default;
			return;
		}

		if (ImGui::IsMouseClicked(0) && s_dragNode.con)
			s_dragState = DragState_Draging;

		break;
	}

	case DragState_BeginDrag:
	{
		break;
	}

	case DragState_Draging:
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		drawList->ChannelsSetCurrent(0); // Background

		drawHermite(drawList, s_dragNode.pos, ImGui::GetIO().MousePos, 12);

		if (!ImGui::IsMouseDown(0))
		{
			ImVec2 pos;
			Connection* con = getHoverCon(offset, &pos);

			// Make sure we are still hovering the same node

			if (con == s_dragNode.con)
			{
				s_dragNode.con = 0;
				s_dragState = DragState_Default;
				return;
			}

			// Lets connect the nodes.
			// TODO: Make sure we connect stuff in the correct way!

			con->input = s_dragNode.con;
			s_dragNode.con = 0;
			s_dragState = DragState_Default;
		}

		break;
	}

	case DragState_Connect:
	{
		break;
	}
	}
}


static void displayNode(ImDrawList* drawList, ImVec2 offset, NodeGraph* node, int& node_selected)
{
	int node_hovered_in_scene = -1;
	bool open_context_menu = false;

	ImGui::PushID(node->id);
	ImVec2 node_rect_min = offset + node->pos;

	// Display node contents first
	drawList->ChannelsSetCurrent(1); // Foreground
	bool old_any_active = ImGui::IsAnyItemActive();

	// Draw title in center

	ImVec2 textSize = ImGui::CalcTextSize(node->name);

	ImVec2 pos = node_rect_min + NODE_WINDOW_PADDING;
	pos.x = node_rect_min.x + (node->size.x / 2) - textSize.x / 2;

	ImGui::SetCursorScreenPos(pos);
	//ImGui::BeginGroup(); // Lock horizontal position
	ImGui::Text("%s", node->name);
	//ImGui::SliderFloat("##value", &node->Value, 0.0f, 1.0f, "Alpha %.2f");
	//float dummy_color[3] = { node->Pos.x / ImGui::GetWindowWidth(), node->Pos.y / ImGui::GetWindowHeight(), fmodf((float)node->ID * 0.5f, 1.0f) };
	//ImGui::ColorEdit3("##color", &dummy_color[0]);

	// Save the size of what we have emitted and weither any of the widgets are being used
	bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
	//node->size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
	ImVec2 node_rect_max = node_rect_min + node->size;

	// Display node box
	drawList->ChannelsSetCurrent(0); // Background

	ImGui::SetCursorScreenPos(node_rect_min);
	ImGui::InvisibleButton("node", node->size);

	if (ImGui::IsItemHovered())
	{
		node_hovered_in_scene = node->id;
		open_context_menu |= ImGui::IsMouseClicked(1);
	}

	bool node_moving_active = false;

	if (ImGui::IsItemActive() && !s_dragNode.con)
		node_moving_active = true;

	ImU32 node_bg_color = node_hovered_in_scene == node->id ? ImColor(75, 75, 75) : ImColor(60, 60, 60);
	drawList->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);

	ImVec2 titleArea = node_rect_max;
	titleArea.y = node_rect_min.y + 30.0f;

	// Draw text bg area
	drawList->AddRectFilled(node_rect_min + ImVec2(1, 1), titleArea, ImColor(100, 0, 0), 4.0f);
	drawList->AddRect(node_rect_min, node_rect_max, ImColor(100, 100, 100), 4.0f);

	ImVec2 off;

	offset.y += 40.0f;

	offset = offset + node_rect_min;

	off.x = node_rect_min.x;
	off.y = node_rect_min.y;

	for (Connection* con : node->inputConnections)
	{
		ImGui::SetCursorScreenPos(offset + ImVec2(10.0f, 0));
		ImGui::Text("%s", con->desc.name);

		ImColor conColor = ImColor(150, 150, 150);

		if (isConnectorHovered(con, node_rect_min))
			conColor = ImColor(200, 200, 200);

		drawList->AddCircleFilled(node_rect_min + con->pos, NODE_SLOT_RADIUS, conColor);

		offset.y += textSize.y + 2.0f;
	}

	offset = node_rect_min;
	offset.y += 40.0f;

	for (Connection* con : node->outputConnections)
	{
		textSize = ImGui::CalcTextSize(con->desc.name);

		ImGui::SetCursorScreenPos(offset + ImVec2(con->pos.x - (textSize.x + 10.0f), 0));
		ImGui::Text("%s", con->desc.name);

		ImColor conColor = ImColor(150, 150, 150);

		if (isConnectorHovered(con, node_rect_min))
			conColor = ImColor(200, 200, 200);

		drawList->AddCircleFilled(node_rect_min + con->pos, NODE_SLOT_RADIUS, conColor);

		offset.y += textSize.y + 2.0f;
	}


	//for (int i = 0; i < node->outputConnections.size(); ++i)
	//	drawList->AddCircleFilled(offset + node->outputSlotPos(i), NODE_SLOT_RADIUS, ImColor(150,150,150,150));

	if (node_widgets_active || node_moving_active)
		node_selected = node->id;

	if (node_moving_active && ImGui::IsMouseDragging(0))
		node->pos = node->pos + ImGui::GetIO().MouseDelta;

	//ImGui::EndGroup();

	ImGui::PopID();
}

NodeGraph* findNodeByCon(Connection* findCon)
{
	for (NodeGraph* node : s_nodes)
	{
		for (Connection* con : node->inputConnections)
		{
			if (con == findCon)
				return node;
		}

		for (Connection* con : node->outputConnections)
		{
			if (con == findCon)
				return node;
		}
	}

	return 0;
}


void renderLines(ImDrawList* drawList, ImVec2 offset)
{
	for (NodeGraph* node : s_nodes)
	{
		for (Connection* con : node->inputConnections)
		{
			if (!con->input)
				continue;

			NodeGraph* targetNode = findNodeByCon(con->input);

			if (!targetNode)
				continue;

			drawHermite(drawList,
				offset + targetNode->pos + con->input->pos,
				offset + node->pos + con->pos,
				12);
		}
	}
}

namespace Razor
{

	MaterialEditor::MaterialEditor(Editor* editor) : EditorComponent(editor)
	{
		active = false;
	}

	MaterialEditor::~MaterialEditor()
	{
	}

	void MaterialEditor::render(float delta)
	{
		ImGui::Begin("Material editor");


		bool open_context_menu = false;
		int node_hovered_in_list = -1;
		int node_hovered_in_scene = -1;

		static int node_selected = -1;
		static ImVec2 scrolling = ImVec2(0.0f, 0.0f);

		ImGui::SameLine();
		ImGui::BeginGroup();

		// Create our child canvas
		//ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", scrolling.x, scrolling.y);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		//ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(40, 40, 40, 200));
		ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
		ImGui::PushItemWidth(120.0f);


		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->ChannelsSplit(2);
		//ImVec2 offset = ImGui::GetCursorScreenPos() - scrolling;

		//displayNode(draw_list, scrolling, s_emittable, node_selected);
		//displayNode(draw_list, scrolling, s_emitter, node_selected);

		for (NodeGraph* node : s_nodes)
			displayNode(draw_list, scrolling, node, node_selected);

		updateDraging(scrolling);
		renderLines(draw_list, scrolling);

		draw_list->ChannelsMerge();

		// Open context menu
		if (!ImGui::IsAnyItemHovered() 
			&& !editor->getComponentsManager()->getComponent<EditorViewport>("Viewport")->isHovered() 
			&& ImGui::IsMouseClicked(1))
		{
			node_selected = node_hovered_in_list = node_hovered_in_scene = -1;
			open_context_menu = true;
		}
		if (open_context_menu)
		{
			ImGui::OpenPopup("context_menu");
			if (node_hovered_in_list != -1)
				node_selected = node_hovered_in_list;
			if (node_hovered_in_scene != -1)
				node_selected = node_hovered_in_scene;
		}

		// Draw context menu
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		if (ImGui::BeginPopup("context_menu"))
		{
			if (ImGui::MenuItem("Load graph..."))
			{
				/*
				char path[1024];
				if (Dialog_open(path))
				{
					printf("file to load %s\n", path);
				}
				*/
			}

			if (ImGui::MenuItem("Save graph..."))
			{
				/*
				char path[1024];
				if (Dialog_save(path))
				{
					saveNodes(path);
				}
				*/
			}


			/*
			Node* node = node_selected != -1 ? &nodes[node_selected] : NULL;
			ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
			if (node)
			{
				ImGui::Text("Node '%s'", node->Name);
				ImGui::Separator();
				if (ImGui::MenuItem("Rename..", NULL, false, false)) {}
				if (ImGui::MenuItem("Delete", NULL, false, false)) {}
				if (ImGui::MenuItem("Copy", NULL, false, false)) {}
			}
			*/
			//else

			for (int i = 0; i < (int)sizeof_array(s_nodeTypes); ++i)
			{
				if (ImGui::MenuItem(s_nodeTypes[i].name))
				{
					NodeGraph* node = createNodeFromType(ImGui::GetIO().MousePos, &s_nodeTypes[i]);
					s_nodes.push_back(node);
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();

		// Scrolling
		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(2, 0.0f))
			scrolling = scrolling - ImGui::GetIO().MouseDelta;

		ImGui::PopItemWidth();
		ImGui::EndChild();
		//ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
		ImGui::EndGroup();

		ImGui::End();
	}

	void MaterialEditor::onEvent(Event& event)
	{
	}

}
