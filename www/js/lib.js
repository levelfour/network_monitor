var width = window.innerWidth,
    height = window.innerHeight;

var nodes = [];
var links = [];
var max_weight = 500;

var force = d3.layout.force()
  .charge(-120)
  .linkDistance(400)
  .nodes(nodes)
  .links(links)
  .size([width, height])
  .on("tick", tick);

var svg = d3.select("body").append("svg")
  .attr("width", width)
  .attr("height", height);

var node = svg.selectAll(".node"),
    link = svg.selectAll(".link");

status_file = "status.json";
save();

d3.json(status_file, function(error, graph) {
  Array.prototype.push.apply(nodes, graph.nodes);
  Array.prototype.push.apply(links, graph.links);
  draw(true);
});

setInterval(watch, 100);

function weight(v) {
  v = Math.min(v, max_weight);
  v = 15 * (Math.sqrt(v / max_weight) * 5) + 1;
  return v;
}

function link_color(v) {
  var thresh = 50;
  v = Math.min(v, thresh);
  var r = (Math.floor(48 + 1.0 * (255 - 48) * v / thresh)).toString(16);
  var g = (48).toString(16);
  var b = (48).toString(16);
  return "#" + ("0"+r).slice(-2) + ("0"+g).slice(-2) + ("0"+b).slice(-2);
}

function draw(initalize=false) {
  link = link.data(force.links());

  if(initalize) {
    link.enter().append("line")
      .attr("class", "link")
      .style("stroke", function(d) { return link_color(d.value); })
      .style("stroke-width", function(d) { return weight(d.value); });
  } else {
    link.transition()
      .attr("class", "link")
      .style("stroke", function(d) { return link_color(d.value); })
      .style("stroke-width", function(d) { return weight(d.value); });
  }

  link.exit().remove();

  if(initalize) {
    node = node.data(force.nodes());
    
    var g = node.enter()
      .append("g")
      .call(force.drag);

    g.append("circle")
      .attr("class", "node")
      .attr("r", 50);

    g.append("text")
      .attr({
        'text-anchor': "middle",
        'dy': ".35em",
        'fill': "white",
      })
      .text(function(d) { return d.name; });

    node.exit().remove();
  }

  force.start();
}

function tick() {
  link.attr("x1", function(d) { return d.source.x; })
      .attr("y1", function(d) { return d.source.y; })
      .attr("x2", function(d) { return d.target.x; })
      .attr("y2", function(d) { return d.target.y; });
  
  node.attr("transform", function(d) {
    return "translate(" + d.x + "," + d.y + ")";
  });
}

function save() {
  d3.json(status_file, function(error, data) {
    saved_status = JSON.stringify(data.links);
  });
}

function watch() {
  var now = new Date();
  d3.json(status_file + "?timestamp=" + now.getTime(), function(error, graph) {
    var new_status = JSON.stringify(graph.links);

    if(saved_status !== new_status) {
      console.log("update");
      links.length = 0;
      Array.prototype.push.apply(links, graph.links);
      draw();
      saved_status = new_status;
    }
  });
}
