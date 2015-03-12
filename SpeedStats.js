var SecretKey = 'K3Y';
var DefaultNumResult = 10;

var saveJson = require('save-json');

var restify = require('restify');
var userSave = require('save')('user', {engine: saveJson('database.json')});

var server = restify.createServer({ name: 'my-api' })

server
  .use(restify.fullResponse())
  .use(restify.bodyParser())


//Return the players best times for all maps
server.get('/player/:Player', function (req, res, next) {
  userSave.find({ Player: req.params.Player }, function (error, entries) {
    if (error) { return next(new restify.InvalidArgumentError(JSON.stringify(error.errors)))}
 
    if (entries) 
    {
      res.send(entries)
    } 
    else 
    {
      res.send(404)
    }
  })
})



//Return the maps best times
server.get('/map/:Map/:NumResults', function (req, res, next) {
  if (req.params.Map === undefined) {return next(new restify.InvalidArgumentError('Map must be supplied'))}    
  
  userSave.find({ Map: req.params.Map }, function (error, entries) {
    if (error) { return next(new restify.InvalidArgumentError(JSON.stringify(error.errors)))}
 
    if (entries) 
    {
       //sort based on time. This seems problematic on large entries. 
      //TODO: look into sorting pre save, or using somesort of db
      entries.sort(function(a, b)
      {
        return parseFloat(a.Time)-parseFloat(b.Time);
      });

      //shorten the list
      var NumResults = (req.params.NumResults === undefined || isNaN(parseInt(req.params.NumResults))) ? DefaultNumResult : req.params.NumResults;
      entries = entries.slice(0,NumResults);

      res.send(entries);
    } 
    else 
    {
      res.send(404);
    }
  })
})


//New score
server.post('/save/:Map/:Player/:Time/:Key', function (req, res, next) {
  if (req.params.Map === undefined) {return next(new restify.InvalidArgumentError('Map must be supplied'))}
  if (req.params.Player === undefined) {return next(new restify.InvalidArgumentError('Player must be supplied'))}
  if (req.params.Time === undefined) {return next(new restify.InvalidArgumentError('Time must be supplied'))}
  if (req.params.Key === undefined) {return next(new restify.InvalidArgumentError('You are not Raxxy are you???'))}

  //check if time is valid
  if(isNaN(parseFloat(req.params.Time)))  {return next(new restify.InvalidArgumentError('Time must be a valid int'))}

  //check the SecretKey
  if(req.params.Key != SecretKey)  {return next(new restify.InvalidArgumentError('You are not Raxxy are you???'))}


//see if the user has a time in the map already
 userSave.findOne({ Map: req.params.Map , Player: req.params.Player}, function (error, entry) {
    if (error.message !== 'No object found') {
      console.log('Different error');
      return next(error);
    }
 
    if (entry) 
    {
      if(req.params.Time < entry.Time)
      {
        console.log("New best time!!");
        entry.Time = req.params.Time;
        userSave.update(entry, function (error, user) {if (error) return next(new restify.InvalidArgumentError(JSON.stringify(error.errors)))})
      }
      else
      {
        console.log("Old time is better");
      }
    } 
    else  //No entry found. make a new one
    {
      console.log("New time");
      userSave.create(
      { 
        Map: req.params.Map , 
        Player: req.params.Player,
        Time: req.params.Time,
        Date: Date(),
      }, function (error, user) {if (error) return next(new restify.InvalidArgumentError(JSON.stringify(error.errors)))})
    }

    res.send(201)
  })
})



server.listen(process.env.PORT || 3000, function () {
  console.log('%s listening at %s', server.name, server.url)
})


