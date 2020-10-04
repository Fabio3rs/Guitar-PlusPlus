SRC_DIR := .
LSRC_DIR := ./Lua
OBJ_DIR := ./obj
LOBJ_DIR := ./lobj
CPP_FILES := $(wildcard $(SRC_DIR)/*.cpp)
C_FILES := $(wildcard $(SRC_DIR)/Lua/*.c)
OBJ_FILES :=  $(patsubst $(LSRC_DIR)/%.c,$(LOBJ_DIR)/%.o,$(C_FILES)) $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_FILES))
INCLUDES := -I../cereal/include -I../glm -I../glfw-3.3.2/include -I../bass
LDFLAGS := -lpthread -lGL -lGLEW
CPPFLAGS := $(INCLUDES) -g -pipe
CFLAGS := -g -pipe
CXXFLAGS := -g

GuitarPP: $(OBJ_FILES)
	g++ $(LDFLAGS) -o $@ $(LOBJ_DIR)/*.o $(OBJ_DIR)/*.o 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(LOBJ_DIR)/%.o: $(LSRC_DIR)/%.c
	gcc $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

