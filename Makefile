SRCDIR       =  interpret_prs
CXXFLAGS	 =  -O2 -g -Wall -fmessage-length=0 -I../prs -I../boolean -I../ucs -I../interpret_boolean -I../interpret_ucs -I../parse_ucs -I../parse_prs -I../parse_dot -I../parse_expression -I../parse -I../common
SOURCES	    :=  $(shell find $(SRCDIR) -name '*.cpp')
OBJECTS	    :=  $(SOURCES:%.cpp=%.o)
TARGET		 =  lib$(SRCDIR).a

all: $(TARGET)

$(TARGET): $(OBJECTS)
	ar rvs $(TARGET) $(OBJECTS)

%.o: $(SRCDIR)/%.cpp 
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c -o $@ $<
	
clean:
	rm -f $(OBJECTS) $(TARGET)
